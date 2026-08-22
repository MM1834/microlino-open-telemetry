import base64
import json
import os
import re
import time
from decimal import Decimal, InvalidOperation

import boto3
from botocore.exceptions import ClientError


TOPIC_ROOT = os.environ.get("TOPIC_ROOT", "mot-test").strip("/")
TEST_VEHICLE_ID = os.environ.get("TEST_VEHICLE_ID", "").strip()
HISTORY_TABLE_NAME = os.environ["HISTORY_TABLE_NAME"]
RETENTION_DAYS = min(7, max(1, int(os.environ.get("RETENTION_DAYS", "3"))))
MAX_BATCH_SAMPLES = min(120, max(1, int(os.environ.get("MAX_BATCH_SAMPLES", "60"))))
MAX_SAMPLE_AGE_SECONDS = min(
    7 * 86400,
    max(300, int(os.environ.get("MAX_SAMPLE_AGE_SECONDS", "259200"))),
)
MAX_FUTURE_SKEW_SECONDS = min(
    300,
    max(0, int(os.environ.get("MAX_FUTURE_SKEW_SECONDS", "60")),),
)

BACKFILL_SUFFIX = "history/backfill/v1"
ACK_SUFFIX = "history/backfill/ack/v1"
BATCH_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._:-]{0,63}$")
SIGNAL_INTERVAL_SECONDS = {"soc": 300, "speed": 60}

history = boto3.resource("dynamodb").Table(HISTORY_TABLE_NAME)
iot_data = boto3.client("iot-data")


class BackfillRejected(ValueError):
    pass


def _decode_payload(encoded):
    try:
        raw = base64.b64decode(encoded or "", validate=True)
    except (ValueError, TypeError) as error:
        raise BackfillRejected("invalid_payload_encoding") from error
    if not raw or len(raw) > 8192:
        raise BackfillRejected("invalid_payload_size")
    try:
        value = json.loads(raw.decode("utf-8"), parse_float=Decimal)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise BackfillRejected("invalid_payload_json") from error
    if not isinstance(value, dict):
        raise BackfillRejected("invalid_envelope")
    return value


def _parse_topic(topic):
    parts = str(topic or "").split("/")
    if len(parts) != 5 or parts[0] != TOPIC_ROOT:
        raise BackfillRejected("invalid_topic")
    vehicle_id = parts[1]
    if "/".join(parts[2:]) != BACKFILL_SUFFIX:
        raise BackfillRejected("invalid_topic")
    if not TEST_VEHICLE_ID or vehicle_id != TEST_VEHICLE_ID:
        raise BackfillRejected("vehicle_not_allowed")
    return vehicle_id


def _number(value, signal):
    if isinstance(value, bool):
        raise BackfillRejected("invalid_sample_value")
    try:
        number = Decimal(str(value))
    except (InvalidOperation, TypeError, ValueError) as error:
        raise BackfillRejected("invalid_sample_value") from error
    if not number.is_finite():
        raise BackfillRejected("invalid_sample_value")
    if signal == "soc" and (number < 0 or number > 100):
        raise BackfillRejected("invalid_sample_value")
    if signal == "speed" and (number < 0 or number > 200):
        raise BackfillRejected("invalid_sample_value")
    return number


def _validate_envelope(envelope, vehicle_id, now_seconds):
    if envelope.get("version") != 1:
        raise BackfillRejected("unsupported_version")
    if envelope.get("vehicleId") != vehicle_id:
        raise BackfillRejected("vehicle_mismatch")
    batch_id = str(envelope.get("batchId", ""))
    if not BATCH_ID_PATTERN.fullmatch(batch_id):
        raise BackfillRejected("invalid_batch_id")
    samples = envelope.get("samples")
    if not isinstance(samples, list) or not 1 <= len(samples) <= MAX_BATCH_SAMPLES:
        raise BackfillRejected("invalid_sample_count")

    validated = []
    previous_timestamp = 0
    minimum_ms = (now_seconds - MAX_SAMPLE_AGE_SECONDS) * 1000
    maximum_ms = (now_seconds + MAX_FUTURE_SKEW_SECONDS) * 1000
    for sample in samples:
        if not isinstance(sample, dict) or set(sample) != {"signal", "sampledAt", "value"}:
            raise BackfillRejected("invalid_sample")
        signal = sample.get("signal")
        if signal not in SIGNAL_INTERVAL_SECONDS:
            raise BackfillRejected("invalid_signal")
        sampled_at = sample.get("sampledAt")
        if isinstance(sampled_at, bool) or not isinstance(sampled_at, int):
            raise BackfillRejected("invalid_sample_timestamp")
        if sampled_at < minimum_ms or sampled_at > maximum_ms:
            raise BackfillRejected("sample_timestamp_out_of_range")
        if sampled_at < previous_timestamp:
            raise BackfillRejected("samples_not_ordered")
        previous_timestamp = sampled_at
        validated.append((signal, sampled_at, _number(sample.get("value"), signal)))
    return batch_id, validated


def _store(vehicle_id, batch_id, samples, received_at):
    stored = 0
    duplicates = 0
    for index, (signal, sampled_at, value) in enumerate(samples):
        sampled_seconds = sampled_at // 1000
        interval = SIGNAL_INTERVAL_SECONDS[signal]
        bucket = sampled_seconds - (sampled_seconds % interval)
        if signal == "speed" and value == 0:
            # Preserve the terminal zero inside an already sampled active minute.
            bucket = sampled_seconds
        try:
            history.put_item(
                Item={
                    "vehicleId": vehicle_id,
                    "sampleKey": f"{signal}#{bucket:010d}",
                    "signal": signal,
                    "sampledAt": sampled_at,
                    "receivedAt": received_at,
                    "value": value,
                    "valueType": "number",
                    "source": "offline-backfill-v1",
                    "batchId": batch_id,
                    "batchIndex": index,
                    "expiresAt": sampled_seconds + RETENTION_DAYS * 86400,
                },
                ConditionExpression="attribute_not_exists(sampleKey)",
            )
            stored += 1
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") == "ConditionalCheckFailedException":
                duplicates += 1
                continue
            raise
    return stored, duplicates


def _publish_ack(vehicle_id, payload):
    iot_data.publish(
        topic=f"{TOPIC_ROOT}/{vehicle_id}/{ACK_SUFFIX}",
        qos=1,
        retain=False,
        payload=json.dumps(payload, separators=(",", ":")).encode("utf-8"),
    )


def handler(event, context):
    topic = event.get("mqttTopic", "")
    received_at = int(event.get("receivedAt") or time.time() * 1000)
    vehicle_id = ""
    batch_id = ""
    try:
        vehicle_id = _parse_topic(topic)
        envelope = _decode_payload(event.get("payloadBase64"))
        candidate_batch_id = str(envelope.get("batchId", ""))
        if BATCH_ID_PATTERN.fullmatch(candidate_batch_id):
            batch_id = candidate_batch_id
        batch_id, samples = _validate_envelope(envelope, vehicle_id, received_at // 1000)
        stored, duplicates = _store(vehicle_id, batch_id, samples, received_at)
        acknowledgement = {
            "version": 1,
            "batchId": batch_id,
            "accepted": True,
            "sampleCount": len(samples),
            "stored": stored,
            "duplicates": duplicates,
        }
        _publish_ack(vehicle_id, acknowledgement)
        print(json.dumps({"eventType": "cache_backfill_accepted", **acknowledgement}))
        return acknowledgement
    except BackfillRejected as error:
        result = {
            "version": 1,
            "batchId": batch_id,
            "accepted": False,
            "reason": str(error),
        }
        # Only acknowledge on the strictly parsed/allowlisted vehicle topic.
        if vehicle_id:
            _publish_ack(vehicle_id, result)
        print(json.dumps({"eventType": "cache_backfill_rejected", **result}))
        return result
