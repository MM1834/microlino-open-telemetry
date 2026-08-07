"""NTF-001 telemetry consumer and SNS dispatcher."""

import base64
import hashlib
import json
import os
import time
from decimal import Decimal

import boto3
from boto3.dynamodb.conditions import Key
from botocore.exceptions import ClientError

from notification_state import ChargingSessionState, apply_update, crossed_threshold


dynamodb = boto3.resource("dynamodb")
preferences = dynamodb.Table(os.environ["PREFERENCE_TABLE_NAME"])
sessions = dynamodb.Table(os.environ["SESSION_TABLE_NAME"])
events = dynamodb.Table(os.environ["EVENT_TABLE_NAME"])
sns = boto3.client("sns")
email_topic_arn = os.environ["EMAIL_TOPIC_ARN"]
event_retention_days = min(31, max(1, int(os.environ.get("EVENT_RETENTION_DAYS", "31"))))

RELEVANT_SUFFIXES = {
    "charging/plugged", "charging/is_charging", "display/soc"
}


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
        previous_soc=_number(item.get("previousSoc")),
        last_plugged_at=int(item.get("lastPluggedAt", 0)),
        last_charging_at=int(item.get("lastChargingAt", 0)),
        last_soc_at=int(item.get("lastSocAt", 0)),
    )


def _item(vehicle_id, state, version):
    item = {
        "vehicleId": vehicle_id,
        "version": version,
        "plugged": state.plugged,
        "chargingObserved": state.charging_observed,
        "lastPluggedAt": state.last_plugged_at,
        "lastChargingAt": state.last_charging_at,
        "lastSocAt": state.last_soc_at,
        "updatedAt": int(time.time() * 1000),
    }
    if state.session_id is not None:
        item["sessionId"] = state.session_id
    if state.previous_soc is not None:
        item["previousSoc"] = Decimal(str(state.previous_soc))
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
                "Item": _item(vehicle_id, after, version + 1),
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


def list_preferences(vehicle_id):
    result = preferences.query(
        KeyConditionExpression=Key("vehicleId").eq(vehicle_id),
        ConsistentRead=True,
    )
    return [item for item in result.get("Items", []) if item.get("enabled") is True]


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


def handler(event, context):
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
    before, after = update_session(vehicle_id, suffix, value, received_at)
    dispatched = 0
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
    return {"accepted": True, "relevant": True, "deliveries": dispatched}
