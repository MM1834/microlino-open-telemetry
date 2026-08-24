"""NTF-001 telemetry consumer and SNS dispatcher."""

import base64
import hashlib
import json
import os
import time
from dataclasses import asdict, fields
from decimal import Decimal

import boto3
from boto3.dynamodb.conditions import Key
from botocore.exceptions import ClientError

from notification_state import (
    CHARGING_STOP_DELAY_MS, ChargingSessionState, apply_update,
    charging_stop_due, crossed_threshold,
)
from journey_state import (
    STOP_DELAY_MS, JourneyState, apply_inactivity_timeout,
    apply_journey_update, clear_journey, summarize_journey,
)


dynamodb = boto3.resource("dynamodb")
preferences = dynamodb.Table(os.environ["PREFERENCE_TABLE_NAME"])
sessions = dynamodb.Table(os.environ["SESSION_TABLE_NAME"])
events = dynamodb.Table(os.environ["EVENT_TABLE_NAME"])
sns = boto3.client("sns")
sqs = boto3.client("sqs")
email_topic_arn = os.environ["EMAIL_TOPIC_ARN"]
charging_stop_queue_url = os.environ.get("CHARGING_STOP_QUEUE_URL", "")
event_retention_days = min(31, max(1, int(os.environ.get("EVENT_RETENTION_DAYS", "31"))))
CHARGING_STOP_DELAY_SECONDS = CHARGING_STOP_DELAY_MS // 1000

CHARGING_SUFFIXES = {"charging/plugged", "charging/is_charging", "display/soc"}
JOURNEY_SUFFIXES = {
    "charging/plugged", "charging/is_charging", "charging/power_signed",
    "bms/vehicle_power_w", "display/odometer_km", "display/soc",
    "display/speed_kmh", "status/online", "journey/energy_counter_id",
    "journey/energy_drawn_wh", "journey/energy_regen_wh",
    "journey/energy_net_wh",
}
RELEVANT_SUFFIXES = CHARGING_SUFFIXES | JOURNEY_SUFFIXES


def _decode(encoded):
    raw = base64.b64decode(encoded or "")
    return json.loads(raw.decode("utf-8"), parse_float=Decimal)


def _number(value):
    if isinstance(value, Decimal):
        return int(value) if value == value.to_integral_value() else float(value)
    return value


def _state(item):
    return ChargingSessionState(
        session_id=item.get("sessionId"),
        plugged=bool(item.get("plugged", False)),
        charging_observed=bool(item.get("chargingObserved", False)),
        is_charging=item.get("isCharging"),
        charging_started_at=int(item.get("chargingStartedAt", 0)),
        stop_candidate_at=int(item.get("stopCandidateAt", 0)),
        previous_soc=_number(item.get("previousSoc")),
        last_plugged_at=int(item.get("lastPluggedAt", 0)),
        last_charging_at=int(item.get("lastChargingAt", 0)),
        last_soc_at=int(item.get("lastSocAt", 0)),
    )


def _item(vehicle_id, state, version, current=None):
    item = dict(current or {})
    item.update({
        "vehicleId": vehicle_id, "version": version,
        "plugged": state.plugged,
        "chargingObserved": state.charging_observed,
        "chargingStartedAt": state.charging_started_at,
        "stopCandidateAt": state.stop_candidate_at,
        "lastPluggedAt": state.last_plugged_at,
        "lastChargingAt": state.last_charging_at,
        "lastSocAt": state.last_soc_at,
        "updatedAt": int(time.time() * 1000),
    })
    item.pop("sessionId", None)
    item.pop("previousSoc", None)
    item.pop("isCharging", None)
    if state.session_id is not None:
        item["sessionId"] = state.session_id
    if state.previous_soc is not None:
        item["previousSoc"] = Decimal(str(state.previous_soc))
    if state.is_charging is not None:
        item["isCharging"] = state.is_charging
    return item


def update_session(vehicle_id, suffix, value, received_at):
    """Optimistically serialize updates for one vehicle."""
    for _ in range(4):
        current = sessions.get_item(
            Key={"vehicleId": vehicle_id}, ConsistentRead=True
        ).get("Item", {})
        version = int(current.get("version", 0))
        before = _state(current)
        after, _ = apply_update(
            before, suffix, _number(value), received_at, threshold=101
        )
        if after == before:
            return before, after
        try:
            kwargs = {
                "Item": _item(vehicle_id, after, version + 1, current),
                "ConditionExpression": "attribute_not_exists(vehicleId)",
            }
            if version:
                kwargs.update({
                    "ConditionExpression": "#version = :version",
                    "ExpressionAttributeNames": {"#version": "version"},
                    "ExpressionAttributeValues": {":version": version},
                })
            sessions.put_item(**kwargs)
            return before, after
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
                raise
    raise RuntimeError("notification session contention")


def list_preferences(vehicle_id, preference_field="enabled"):
    result = preferences.query(
        KeyConditionExpression=Key("vehicleId").eq(vehicle_id),
        ConsistentRead=True,
    )
    return [
        item for item in result.get("Items", [])
        if item.get(preference_field) is True
    ]


def _journey_state(item):
    stored = item.get("journey") or {}
    names = {field.name for field in fields(JourneyState)}
    return JourneyState(**{
        key: _number(value) for key, value in stored.items() if key in names
    })


def _ddb(value):
    if isinstance(value, float):
        return Decimal(str(value))
    if isinstance(value, dict):
        return {key: _ddb(child) for key, child in value.items()}
    return value


def _put_journey(vehicle_id, current, state):
    version = int(current.get("version", 0))
    item = dict(current)
    item.update({
        "vehicleId": vehicle_id, "version": version + 1,
        "journey": _ddb(asdict(state)), "updatedAt": int(time.time() * 1000),
    })
    kwargs = {
        "Item": item, "ConditionExpression": "attribute_not_exists(vehicleId)"
    }
    if version:
        kwargs.update({
            "ConditionExpression": "#version = :version",
            "ExpressionAttributeNames": {"#version": "version"},
            "ExpressionAttributeValues": {":version": version},
        })
    sessions.put_item(**kwargs)


def update_journey(vehicle_id, suffix, value, received_at):
    for _ in range(4):
        current = sessions.get_item(
            Key={"vehicleId": vehicle_id}, ConsistentRead=True
        ).get("Item", {})
        before = _journey_state(current)
        after = apply_journey_update(before, suffix, _number(value), received_at)
        if after == before:
            return after
        try:
            _put_journey(vehicle_id, current, after)
            return after
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
                raise
    raise RuntimeError("journey session contention")


def event_id(user_sub, vehicle_id, session_id, threshold):
    material = f"{user_sub}|{vehicle_id}|{session_id}|{threshold}".encode("utf-8")
    return hashlib.sha256(material).hexdigest()


def reserve_event(preference, vehicle_id, crossing, threshold, received_at):
    identifier = event_id(
        preference["userSub"], vehicle_id, crossing.session_id, threshold
    )
    now = int(time.time())
    try:
        events.put_item(
            Item={
                "eventId": identifier,
                "userSub": preference["userSub"],
                "vehicleId": vehicle_id,
                "sessionId": crossing.session_id,
                "threshold": Decimal(str(threshold)),
                "reachedSoc": Decimal(str(crossing.current_soc)),
                "receivedAt": received_at,
                "createdAt": now * 1000,
                "expiresAt": now + event_retention_days * 86400,
                "status": "RESERVED",
            },
            ConditionExpression="attribute_not_exists(eventId)",
        )
        return identifier
    except ClientError as error:
        if error.response.get("Error", {}).get("Code") == "ConditionalCheckFailedException":
            return None
        raise


def dispatch(preference, identifier, vehicle_id, threshold, reached_soc):
    vehicle_name = str(preference.get("vehicleName") or vehicle_id)[:40]
    text = (
        f"MOT: {vehicle_name} ({vehicle_id}) hat beim Laden "
        f"{reached_soc:g}% SOC erreicht (Ziel {threshold:g}%). "
        "Info, keine Ladesteuerung."
    )
    deliveries = []
    # SNS itself suppresses delivery while the email subscription is pending.
    if preference.get("emailEnabled"):
        result = sns.publish(
            TopicArn=email_topic_arn,
            Subject=f"MOT - {vehicle_id} hat {threshold:g}% erreicht"[:100],
            Message=text,
            MessageAttributes={
                "recipientKey": {
                    "DataType": "String",
                    "StringValue": str(preference["recipientKey"]),
                }
            },
        )
        deliveries.append({"channel": "EMAIL", "messageId": result["MessageId"]})
    if preference.get("smsEnabled") and preference.get("smsConfirmed"):
        result = sns.publish(
            PhoneNumber=preference["phoneE164"],
            Message=text,
            MessageAttributes={
                "AWS.SNS.SMS.SMSType": {
                    "DataType": "String", "StringValue": "Transactional"
                }
            },
        )
        deliveries.append({"channel": "SMS", "messageId": result["MessageId"]})
    events.update_item(
        Key={"eventId": identifier},
        UpdateExpression="SET #status=:status, deliveries=:deliveries",
        ExpressionAttributeNames={"#status": "status"},
        ExpressionAttributeValues={
            ":status": "DISPATCHED" if deliveries else "NO_CONFIRMED_CHANNEL",
            ":deliveries": deliveries,
        },
    )
    return deliveries


def enqueue_charging_stop(vehicle_id, state):
    if not charging_stop_queue_url or not state.session_id or not state.stop_candidate_at:
        return False
    sqs.send_message(
        QueueUrl=charging_stop_queue_url,
        DelaySeconds=CHARGING_STOP_DELAY_SECONDS,
        MessageBody=json.dumps({
            "vehicleId": vehicle_id,
            "sessionId": state.session_id,
            "candidateAt": state.stop_candidate_at,
        }, separators=(",", ":")),
    )
    return True


def charging_stop_event_id(preference, vehicle_id, state, threshold):
    material = (
        f'{preference["userSub"]}|{vehicle_id}|charging-stop|'
        f'{state.session_id}'
    ).encode("utf-8")
    return hashlib.sha256(material).hexdigest()


def reserve_charging_stop_event(preference, vehicle_id, state, threshold, now_ms):
    identifier = charging_stop_event_id(preference, vehicle_id, state, threshold)
    now = int(time.time())
    try:
        events.put_item(
            Item={
                "eventId": identifier, "eventType": "CHARGING_STOP",
                "userSub": preference["userSub"], "vehicleId": vehicle_id,
                "sessionId": state.session_id,
                "candidateAt": state.stop_candidate_at,
                "threshold": Decimal(str(threshold)),
                "stoppedSoc": Decimal(str(state.previous_soc)),
                "receivedAt": now_ms, "createdAt": now * 1000,
                "expiresAt": now + event_retention_days * 86400,
                "status": "RESERVED",
            },
            ConditionExpression="attribute_not_exists(eventId)",
        )
        return identifier
    except ClientError as error:
        if error.response.get("Error", {}).get("Code") == "ConditionalCheckFailedException":
            return None
        raise


def dispatch_charging_stop(preference, identifier, vehicle_id, state, threshold):
    vehicle_name = str(preference.get("vehicleName") or vehicle_id)[:40]
    text = (
        f"MOT: Der Ladevorgang von {vehicle_name} ({vehicle_id}) ist bei "
        f"{state.previous_soc:g}% SOC seit mindestens 60 Sekunden gestoppt "
        f"(Ladestopp-Ziel {threshold:g}%). Das Fahrzeug ist weiterhin eingesteckt. "
        "Dies kann auch ein manueller Stopp oder externes Lastmanagement sein. "
        "Info, keine Ladesteuerung."
    )
    deliveries = []
    if preference.get("emailEnabled"):
        result = sns.publish(
            TopicArn=email_topic_arn,
            Subject=f"MOT - Ladestopp bei {state.previous_soc:g}% ({vehicle_id})"[:100],
            Message=text,
            MessageAttributes={
                "recipientKey": {
                    "DataType": "String",
                    "StringValue": str(preference["recipientKey"]),
                }
            },
        )
        deliveries.append({"channel": "EMAIL", "messageId": result["MessageId"]})
    events.update_item(
        Key={"eventId": identifier},
        UpdateExpression="SET #status=:status, deliveries=:deliveries",
        ExpressionAttributeNames={"#status": "status"},
        ExpressionAttributeValues={
            ":status": "DISPATCHED" if deliveries else "NO_CONFIRMED_CHANNEL",
            ":deliveries": deliveries,
        },
    )
    return deliveries


def validate_charging_stop(message, now_ms):
    vehicle_id = str(message.get("vehicleId", ""))
    session_id = str(message.get("sessionId", ""))
    candidate_at = int(message.get("candidateAt", 0))
    if not vehicle_id or not session_id or candidate_at <= 0:
        return 0
    item = sessions.get_item(
        Key={"vehicleId": vehicle_id}, ConsistentRead=True
    ).get("Item", {})
    state = _state(item)
    dispatched = 0
    for preference in list_preferences(vehicle_id, "chargingStopEmailEnabled"):
        threshold = float(preference.get("chargingStopThreshold", 80))
        if not charging_stop_due(
            state, session_id, candidate_at, now_ms, threshold
        ):
            continue
        identifier = reserve_charging_stop_event(
            preference, vehicle_id, state, threshold, now_ms
        )
        if identifier:
            dispatched += len(dispatch_charging_stop(
                preference, identifier, vehicle_id, state, threshold
            ))
    return dispatched


def journey_event_id(user_sub, vehicle_id, journey_id):
    material = f"{user_sub}|{vehicle_id}|journey|{journey_id}".encode("utf-8")
    return hashlib.sha256(material).hexdigest()


def reserve_journey_event(preference, vehicle_id, summary):
    identifier = journey_event_id(
        preference["userSub"], vehicle_id, summary.journey_id
    )
    now = int(time.time())
    try:
        events.put_item(
            Item={
                "eventId": identifier, "eventType": "JOURNEY_SUMMARY",
                "userSub": preference["userSub"], "vehicleId": vehicle_id,
                "journeyId": summary.journey_id,
                "distanceKm": Decimal(str(summary.distance_km)),
                "socUsed": Decimal(str(summary.soc_used)),
                "energyNetKwh": Decimal(str(summary.energy_net_kwh)),
                "energySource": summary.energy_source,
                "completionTrigger": summary.completion_trigger,
                "receivedAt": summary.ended_at, "createdAt": now * 1000,
                "expiresAt": now + event_retention_days * 86400,
                "status": "RESERVED",
            },
            ConditionExpression="attribute_not_exists(eventId)",
        )
        return identifier
    except ClientError as error:
        if error.response.get("Error", {}).get("Code") == "ConditionalCheckFailedException":
            return None
        raise


def _de(value, digits=1):
    return f"{value:.{digits}f}".replace(".", ",")


def dispatch_journey(preference, identifier, vehicle_id, summary):
    vehicle_name = str(preference.get("vehicleName") or vehicle_id)[:40]
    timeout_note = (
        "Fahrtende: 30-Min.-Telemetrie-Timeout "
        "(Werte bis zum letzten empfangenen Signal)\n\n"
        if summary.completion_trigger == "telemetry_timeout" else ""
    )
    text = (
        f"MOT: Fahrt mit {vehicle_name} ({vehicle_id}) abgeschlossen.\n\n"
        f"Strecke: {_de(summary.distance_km)} km\n"
        f"Fahrzeit: {summary.duration_minutes} min\n"
        f"Verbrauchter SOC: {_de(summary.soc_used)} %-Punkte\n"
        f"Energie bezogen: {_de(summary.energy_drawn_kwh, 2)} kWh\n"
        f"Rekuperiert: {_de(summary.energy_regen_kwh, 2)} kWh\n"
        f"Verbrauchte Netto-Leistung: {_de(summary.energy_net_kwh, 2)} kWh\n"
        f"Nettoverbrauch: {_de(summary.net_kwh_per_100_km, 1)} kWh/100 km\n\n"
        f"{timeout_note}"
        f"Energiequelle: {summary.source_flag}\n"
        "Info, keine Abrechnungs- oder Präzisionsmessung."
    )
    deliveries = []
    if preference.get("emailEnabled"):
        result = sns.publish(
            TopicArn=email_topic_arn,
            Subject=f"MOT - Fahrt {summary.distance_km:.1f} km mit {vehicle_name}"[:100],
            Message=text,
            MessageAttributes={
                "recipientKey": {
                    "DataType": "String",
                    "StringValue": str(preference["recipientKey"]),
                }
            },
        )
        deliveries.append({"channel": "EMAIL", "messageId": result["MessageId"]})
    events.update_item(
        Key={"eventId": identifier},
        UpdateExpression="SET #status=:status, deliveries=:deliveries",
        ExpressionAttributeNames={"#status": "status"},
        ExpressionAttributeValues={
            ":status": "DISPATCHED" if deliveries else "NO_CONFIRMED_CHANNEL",
            ":deliveries": deliveries,
        },
    )
    return deliveries


def finalize_journey(vehicle_id, now_ms):
    """Atomically close one due journey and emit each user's email once."""
    for _ in range(4):
        current = sessions.get_item(
            Key={"vehicleId": vehicle_id}, ConsistentRead=True
        ).get("Item", {})
        state = apply_inactivity_timeout(_journey_state(current), now_ms)
        summary, reason = summarize_journey(state, now_ms)
        if reason == "not_due":
            return 0, reason
        try:
            _put_journey(
                vehicle_id, current,
                clear_journey(state, reason=reason, completed_at=now_ms),
            )
            break
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
                raise
    else:
        raise RuntimeError("journey finalization contention")
    if not summary:
        return 0, reason
    dispatched = 0
    for preference in list_preferences(vehicle_id, "journeyEmailEnabled"):
        identifier = reserve_journey_event(preference, vehicle_id, summary)
        if identifier:
            dispatched += len(dispatch_journey(
                preference, identifier, vehicle_id, summary
            ))
    return dispatched, reason


def finalize_due_journeys(now_ms):
    dispatched = 0
    evaluated = 0
    scan = {}
    while True:
        result = sessions.scan(**scan)
        for item in result.get("Items", []):
            state = _journey_state(item)
            if state.active_id:
                count, _ = finalize_journey(item["vehicleId"], now_ms)
                dispatched += count
                evaluated += 1
        key = result.get("LastEvaluatedKey")
        if not key:
            break
        scan["ExclusiveStartKey"] = key
    return {"accepted": True, "scheduled": True, "evaluated": evaluated,
            "deliveries": dispatched}


def handler(event, context):
    if event.get("Records") and all(
        record.get("eventSource") == "aws:sqs" for record in event["Records"]
    ):
        dispatched = 0
        for record in event["Records"]:
            dispatched += validate_charging_stop(
                json.loads(record.get("body") or "{}"), int(time.time() * 1000)
            )
        return {"accepted": True, "delayed": True, "deliveries": dispatched}
    if event.get("source") == "aws.events":
        return finalize_due_journeys(int(time.time() * 1000))
    topic = str(event.get("mqttTopic", ""))
    parts = topic.split("/")
    if len(parts) < 3 or parts[0] != "mot":
        return {"accepted": False, "reason": "invalid_topic"}
    vehicle_id = parts[1]
    suffix = "/".join(parts[2:])
    if suffix not in RELEVANT_SUFFIXES:
        return {"accepted": True, "relevant": False}
    received_at = int(event.get("receivedAt") or time.time() * 1000)
    value = _decode(event.get("payloadBase64"))
    dispatched = 0
    if suffix in CHARGING_SUFFIXES:
        before, after = update_session(vehicle_id, suffix, value, received_at)
        if (
            suffix == "charging/is_charging"
            and value is False
            and after.stop_candidate_at == received_at
        ):
            enqueue_charging_stop(vehicle_id, after)
    if suffix == "display/soc":
        for preference in list_preferences(vehicle_id):
            threshold = float(preference.get("threshold", 80))
            crossing = crossed_threshold(before, after, threshold)
            if not crossing:
                continue
            identifier = reserve_event(
                preference, vehicle_id, crossing, threshold, received_at
            )
            if identifier:
                dispatched += len(dispatch(
                    preference, identifier, vehicle_id, threshold, crossing.current_soc
                ))
    if suffix in JOURNEY_SUFFIXES:
        state = update_journey(vehicle_id, suffix, value, received_at)
        if (
            state.active_id and state.stopped_at
            and (
                state.charging_after_stop
                or received_at - state.stopped_at >= STOP_DELAY_MS
            )
        ):
            completed, _ = finalize_journey(vehicle_id, received_at)
            dispatched += completed
    return {"accepted": True, "relevant": True, "deliveries": dispatched}
