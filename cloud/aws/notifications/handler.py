"""NTF-001 telemetry consumer and SNS dispatcher."""

import base64
import hashlib
import json
import os
import random
import re
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
from charging_summary_state import (
    SUMMARY_DELAY_MS, ChargingSummaryState, apply as apply_charging_summary,
    complete as complete_charging_summary,
    delayed_due as charging_summary_due,
)
from charging_display_state import (
    ChargingDisplayState, apply as apply_charging_display,
    attach_capacity as attach_charging_display_capacity,
)
from drive_since_charge_state import (
    DriveSinceChargeState, accumulate as accumulate_drive_since_charge,
    measurement_from_journey,
)
from daily_summary import aggregate as aggregate_daily
from daily_summary import has_activity as daily_has_activity
from daily_summary import recent_movement as daily_recent_movement
from daily_summary import report_window
from email_templates import (
    charging_stop as charging_stop_email,
    charging_summary as charging_summary_email,
    daily_summary as daily_summary_email,
    journey_summary as journey_summary_email,
    sms_charging_stop,
    sms_soc_target,
    soc_target as soc_target_email,
)
from journey_state import (
    STOP_DELAY_MS, JourneyState, apply_inactivity_timeout,
    apply_journey_update, clear_journey, summarize_journey,
)


dynamodb = boto3.resource("dynamodb")
preferences = dynamodb.Table(os.environ["PREFERENCE_TABLE_NAME"])
sessions = dynamodb.Table(os.environ["SESSION_TABLE_NAME"])
events = dynamodb.Table(os.environ["EVENT_TABLE_NAME"])
vehicle_profiles = dynamodb.Table(os.environ.get("VEHICLE_PROFILE_TABLE_NAME", "disabled"))
sms_approvals = dynamodb.Table(os.environ.get("SMS_APPROVAL_TABLE_NAME", "disabled"))
sms_destinations = dynamodb.Table(os.environ.get("SMS_DESTINATION_TABLE_NAME", "disabled"))
sms_rate = dynamodb.Table(os.environ.get("SMS_RATE_TABLE_NAME", "disabled"))
sns = boto3.client("sns")
sqs = boto3.client("sqs")
sms_voice = boto3.client("pinpoint-sms-voice-v2")
cloudwatch = boto3.client("cloudwatch")
email_topic_arn = os.environ["EMAIL_TOPIC_ARN"]
charging_stop_queue_url = os.environ.get("CHARGING_STOP_QUEUE_URL", "")
event_retention_days = min(31, max(1, int(os.environ.get("EVENT_RETENTION_DAYS", "31"))))
CHARGING_STOP_DELAY_SECONDS = CHARGING_STOP_DELAY_MS // 1000
CHARGING_SUMMARY_DELAY_SECONDS = SUMMARY_DELAY_MS // 1000
SMS_PHONE = re.compile(r"^\+(41|49)[1-9][0-9]{7,11}$")
sms_delivery_enabled = os.environ.get("SMS_DELIVERY_ENABLED", "false") == "true"
sms_sender_arn = os.environ.get("SMS_SENDER_ARN", "")
sms_sender_arn_de = os.environ.get("SMS_SENDER_ARN_DE", "")
sms_configuration_set = os.environ.get("SMS_CONFIGURATION_SET", "")
sms_spend_alarm_name = os.environ.get("SMS_SPEND_ALARM_NAME", "")
sms_expected_spend_limit = Decimal(os.environ.get("SMS_EXPECTED_SPEND_LIMIT_USD", "1"))

CHARGING_SUFFIXES = {
    "charging/plugged", "charging/is_charging", "display/soc",
    "display/odometer_km", "display/odo",
}
CHARGING_DISPLAY_SUFFIXES = {
    "charging/plugged", "charging/is_charging", "charging/power_signed",
    "bms/vehicle_power_w", "display/soc", "display/odometer_km",
    "display/odo", "display/speed_kmh",
}
JOURNEY_SUFFIXES = {
    "charging/plugged", "charging/is_charging", "charging/power_signed",
    "bms/vehicle_power_w", "display/odometer_km", "display/soc",
    "display/speed_kmh", "status/online", "journey/energy_counter_id",
    "journey/energy_drawn_wh", "journey/energy_regen_wh",
    "journey/energy_net_wh",
}
RELEVANT_SUFFIXES = CHARGING_SUFFIXES | CHARGING_DISPLAY_SUFFIXES | JOURNEY_SUFFIXES
JOURNEY_SESSION_PREFIX = "journey#"
JOURNEY_UPDATE_ATTEMPTS = 12


def _decode(encoded, allow_plain_counter_id=False):
    raw = base64.b64decode(encoded or "")
    text = raw.decode("utf-8")
    try:
        return json.loads(text, parse_float=Decimal)
    except json.JSONDecodeError:
        if allow_plain_counter_id and re.fullmatch(r"[A-Za-z0-9_-]{1,80}", text):
            return text
        raise


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


def _summary_state(item):
    stored = item.get("chargingSummary") or {}
    names = {field.name for field in fields(ChargingSummaryState)}
    return ChargingSummaryState(**{
        key: _number(value) for key, value in stored.items() if key in names
    })


def _charging_display_state(item):
    stored = item.get("chargingDisplay") or {}
    names = {field.name for field in fields(ChargingDisplayState)}
    return ChargingDisplayState(**{
        key: _number(value) for key, value in stored.items() if key in names
    })


def update_charging_states(vehicle_id, suffix, value, received_at):
    """Persist independent email and display charging states in one write."""
    for _ in range(4):
        current = sessions.get_item(Key={"vehicleId": vehicle_id}, ConsistentRead=True).get("Item", {})
        version = int(current.get("version", 0))
        summary_before = _summary_state(current)
        summary_after = (
            complete_charging_summary(
                summary_before, received_at, plugged=False,
                capacity_kwh=_charging_capacity_kwh(vehicle_id),
            )
            if suffix == "charging/plugged" and value is False and summary_before.active
            else apply_charging_summary(summary_before, suffix, _number(value), received_at)
        )
        display_before = _charging_display_state(current)
        display_after = apply_charging_display(
            display_before, suffix, _number(value), received_at
        )
        if display_after.block_open and not display_before.block_open:
            display_after = attach_charging_display_capacity(
                display_after, _charging_capacity_kwh(vehicle_id)
            )
        if summary_after == summary_before and display_after == display_before:
            return summary_before, summary_after
        item = dict(current)
        item.update({
            "vehicleId": vehicle_id,
            "version": version + 1,
            "chargingSummary": _ddb(asdict(summary_after)),
            "chargingDisplay": _ddb(asdict(display_after)),
            "updatedAt": int(time.time() * 1000),
        })
        kwargs = {"Item": item, "ConditionExpression": "attribute_not_exists(vehicleId)"}
        if version:
            kwargs.update({"ConditionExpression": "#version=:version",
                           "ExpressionAttributeNames": {"#version": "version"},
                           "ExpressionAttributeValues": {":version": version}})
        try:
            sessions.put_item(**kwargs)
            return summary_before, summary_after
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
                raise
    raise RuntimeError("charging state contention")


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
        if preference_field is None or item.get(preference_field) is True
    ]


def sms_delivery(preference, vehicle_id, text):
    """Return privacy-safe SMS delivery evidence; every missing gate rejects."""
    if not (preference.get("smsEnabled") is True and preference.get("smsConfirmed") is True):
        return None
    try:
        if not sms_delivery_enabled:
            return {"channel": "SMS", "status": "REJECTED", "reason": "KILL_SWITCH"}
        phone = str(preference.get("phoneE164", ""))
        if not SMS_PHONE.fullmatch(phone):
            return {"channel": "SMS", "status": "REJECTED", "reason": "DESTINATION"}
        if len(text) > 160 or any(ord(character) > 127 for character in text):
            return {"channel": "SMS", "status": "REJECTED", "reason": "MESSAGE_FORMAT"}
        now = int(time.time())
        destination_fingerprint = hashlib.sha256(phone.encode()).hexdigest()
        key = {"vehicleId": vehicle_id, "userSub": preference["userSub"]}
        approval = sms_approvals.get_item(Key=key, ConsistentRead=True).get("Item", {})
        if not (
            approval.get("status") == "ACTIVE"
            and approval.get("destinationFingerprint") == destination_fingerprint
            and approval.get("isoCountryCode") == ("CH" if phone.startswith("+41") else "DE")
            and approval.get("originator") == "MOT"
            and int(approval.get("expiresAt", 0)) > now
        ):
            return {"channel": "SMS", "status": "REJECTED", "reason": "APPROVAL"}
        destination = sms_destinations.get_item(
            Key={"destinationFingerprint": destination_fingerprint}, ConsistentRead=True
        ).get("Item", {})
        if destination.get("status") != "VERIFIED":
            return {"channel": "SMS", "status": "REJECTED", "reason": "VERIFICATION"}
        alarms = cloudwatch.describe_alarms(AlarmNames=[sms_spend_alarm_name]).get("MetricAlarms", [])
        if len(alarms) != 1 or alarms[0].get("StateValue") != "OK":
            return {"channel": "SMS", "status": "REJECTED", "reason": "SPEND_ALARM"}
        limits = sms_voice.describe_spend_limits().get("SpendLimits", [])
        text_limits = [item for item in limits if item.get("Name") == "TEXT_MESSAGE_MONTHLY_SPEND_LIMIT"]
        if len(text_limits) != 1 or Decimal(str(text_limits[0].get("EnforcedLimit", "-1"))) != sms_expected_spend_limit:
            return {"channel": "SMS", "status": "REJECTED", "reason": "SPEND_LIMIT"}
        day = time.strftime("%Y%m%d", time.gmtime(now))
        sms_rate.update_item(
            Key={"rateKey": f"{destination_fingerprint}|{day}"},
            UpdateExpression="SET expiresAt=:expiry ADD #count :one",
            ConditionExpression="attribute_not_exists(#count) OR #count < :maximum",
            ExpressionAttributeNames={"#count": "count"},
            ExpressionAttributeValues={":expiry": now + 172800, ":one": 1, ":maximum": 10},
        )
        result = sms_voice.send_text_message(
            DestinationPhoneNumber=phone,
            OriginationIdentity=sms_sender_arn if phone.startswith("+41") else sms_sender_arn_de,
            MessageBody=text, MessageType="TRANSACTIONAL",
            ConfigurationSetName=sms_configuration_set,
        )
        return {"channel": "SMS", "status": "SENT", "messageId": result["MessageId"]}
    except ClientError as error:
        code = error.response.get("Error", {}).get("Code", "PROVIDER")
        reason = "RATE_LIMIT" if code == "ConditionalCheckFailedException" else "PROVIDER"
        return {"channel": "SMS", "status": "REJECTED", "reason": reason}


def record_deliveries(identifier, deliveries):
    sent = any(item.get("status", "SENT") == "SENT" for item in deliveries)
    events.update_item(
        Key={"eventId": identifier},
        UpdateExpression="SET #status=:status, deliveries=:deliveries",
        ExpressionAttributeNames={"#status": "status"},
        ExpressionAttributeValues={
            ":status": "DISPATCHED" if sent else "NO_DELIVERY",
            ":deliveries": deliveries,
        },
    )


def _journey_state(item):
    stored = item.get("journey") or {}
    names = {field.name for field in fields(JourneyState)}
    return JourneyState(**{
        key: _number(value) for key, value in stored.items() if key in names
    })


def _drive_since_charge_state(item):
    stored = item.get("driveSinceCharge") or {}
    names = {field.name for field in fields(DriveSinceChargeState)}
    return DriveSinceChargeState(**{
        key: _number(value) for key, value in stored.items() if key in names
    })


def _charge_reference_at(vehicle_id):
    item = sessions.get_item(
        Key={"vehicleId": vehicle_id}, ConsistentRead=True
    ).get("Item", {})
    charging = item.get("chargingSummary") or {}
    display = item.get("chargingDisplay") or {}
    candidates = []
    if (
        display.get("last_reference_soc") is not None
        and display.get("last_reference_odometer") is not None
    ):
        candidates.append(int(
            _number(display.get("last_charge_at"))
            or _number(display.get("last_finalized_at")) or 0
        ))
    if (
        charging.get("last_charge_soc") is not None
        and charging.get("last_charge_odometer") is not None
    ):
        candidates.append(int(_number(charging.get("last_charge_at")) or 0))
    return max(candidates or [0])


def _ddb(value):
    if isinstance(value, float):
        return Decimal(str(value))
    if isinstance(value, dict):
        return {key: _ddb(child) for key, child in value.items()}
    return value


def _journey_session_id(vehicle_id):
    return f"{JOURNEY_SESSION_PREFIX}{vehicle_id}"


def _journey_retry_delay(attempt):
    time.sleep(min(0.1, 0.005 * (2 ** min(attempt, 4))) * (0.5 + random.random()))


def _read_journey_session(vehicle_id):
    """Read the isolated Journey item, seeding it once from the legacy item."""
    session_id = _journey_session_id(vehicle_id)
    current = sessions.get_item(
        Key={"vehicleId": session_id}, ConsistentRead=True
    ).get("Item", {})
    if current:
        return current

    legacy = sessions.get_item(
        Key={"vehicleId": vehicle_id}, ConsistentRead=True
    ).get("Item", {})
    seed = {
        "vehicleId": session_id,
        "journeyVehicleId": vehicle_id,
        "version": 1,
        "journey": _ddb(asdict(_journey_state(legacy))),
        "updatedAt": int(time.time() * 1000),
    }
    try:
        sessions.put_item(
            Item=seed,
            ConditionExpression="attribute_not_exists(vehicleId)",
        )
        return seed
    except ClientError as error:
        if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
            raise
    return sessions.get_item(
        Key={"vehicleId": session_id}, ConsistentRead=True
    ).get("Item", {})


def _put_journey(vehicle_id, current, state, drive_since_charge=None):
    version = int(current.get("version", 0))
    item = {
        "vehicleId": _journey_session_id(vehicle_id),
        "journeyVehicleId": vehicle_id,
        "version": version + 1,
        "journey": _ddb(asdict(state)), "updatedAt": int(time.time() * 1000),
    }
    aggregate = (
        drive_since_charge
        if drive_since_charge is not None
        else _drive_since_charge_state(current)
    )
    if aggregate != DriveSinceChargeState():
        item["driveSinceCharge"] = _ddb(asdict(aggregate))
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
    for attempt in range(JOURNEY_UPDATE_ATTEMPTS):
        current = _read_journey_session(vehicle_id)
        before = _journey_state(current)
        normalized = _number(value)
        if (
            suffix == "display/speed_kmh"
            and not isinstance(normalized, bool)
            and isinstance(normalized, (int, float))
            and normalized > 1
            and before.active_id
            and before.stopped_at
            and not before.charging_after_stop
            and received_at - before.stopped_at >= STOP_DELAY_MS
        ):
            return before, True
        after = apply_journey_update(before, suffix, normalized, received_at)
        if after == before:
            return after, False
        try:
            _put_journey(vehicle_id, current, after)
            return after, False
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
                raise
            _journey_retry_delay(attempt)
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
    sms_text = sms_soc_target(preference, vehicle_id, reached_soc, threshold)
    deliveries = []
    # SNS itself suppresses delivery while the email subscription is pending.
    if preference.get("emailEnabled"):
        subject, email_text = soc_target_email(
            preference, vehicle_id, vehicle_name, threshold, reached_soc
        )
        result = sns.publish(
            TopicArn=email_topic_arn,
            Subject=subject[:100], Message=email_text,
            MessageAttributes={
                "recipientKey": {
                    "DataType": "String",
                    "StringValue": str(preference["recipientKey"]),
                }
            },
        )
        deliveries.append({"channel": "EMAIL", "messageId": result["MessageId"]})
    sms_result = sms_delivery(preference, vehicle_id, sms_text)
    if sms_result:
        deliveries.append(sms_result)
    record_deliveries(identifier, deliveries)
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


def enqueue_charging_summary(vehicle_id, state):
    if not charging_stop_queue_url or not state.session_id or not state.stop_candidate_at:
        return False
    sqs.send_message(QueueUrl=charging_stop_queue_url,
        DelaySeconds=CHARGING_SUMMARY_DELAY_SECONDS,
        MessageBody=json.dumps({"type": "charging_summary", "vehicleId": vehicle_id,
            "sessionId": state.session_id, "candidateAt": state.stop_candidate_at}, separators=(",", ":")))
    return True


def _charging_capacity_kwh(vehicle_id):
    if os.environ.get("VEHICLE_PROFILE_TABLE_NAME") is None:
        return None
    item = vehicle_profiles.get_item(
        Key={"vehicleId": vehicle_id}, ConsistentRead=True
    ).get("Item", {})
    value = _number(item.get("batteryCapacityKwh"))
    return float(value) if isinstance(value, (int, float)) and value > 0 else None


def _charging_quality(state, ended_at, capacity_kwh):
    duration_ms = max(1, ended_at - state.started_at)
    coverage = min(100.0, max(0.0, state.covered_power_ms * 100.0 / duration_ms))
    soc_delta = (None if state.start_soc is None or state.last_soc is None
                 else state.last_soc - state.start_soc)
    estimate = (None if capacity_kwh is None or soc_delta is None or soc_delta < 0
                else capacity_kwh * soc_delta / 100.0)
    return coverage, estimate


def dispatch_charging_summary(preference, vehicle_id, state, ended_at, reason,
                              capacity_kwh=None):
    material = f'{preference["userSub"]}|{vehicle_id}|charging-summary|{state.session_id}'.encode()
    identifier = hashlib.sha256(material).hexdigest()
    now = int(time.time())
    try:
        duration = max(1, round((ended_at - state.started_at) / 60000))
        soc_delta = (None if state.start_soc is None or state.last_soc is None
                     else state.last_soc - state.start_soc)
        coverage, soc_estimate = _charging_quality(state, ended_at, capacity_kwh)
        item = {"eventId": identifier, "eventType": "CHARGING_SUMMARY",
            "userSub": preference["userSub"], "vehicleId": vehicle_id,
            "sessionId": state.session_id, "receivedAt": ended_at, "createdAt": now * 1000,
            "durationMinutes": duration,
            "energyChargedKwh": Decimal(str(state.energy_kwh)),
            "energyCoveragePercent": Decimal(str(coverage)),
            "largestPowerGapSeconds": Decimal(str(state.largest_power_gap_ms / 1000.0)),
            "powerSampleCount": state.power_sample_count,
            "expiresAt": now + event_retention_days * 86400, "status": "RECORDED"}
        if capacity_kwh is not None:
            item["batteryCapacityKwh"] = Decimal(str(capacity_kwh))
        if soc_estimate is not None:
            item["socEstimatedEnergyKwh"] = Decimal(str(soc_estimate))
        if state.start_soc is not None:
            item["startSoc"] = Decimal(str(state.start_soc))
        if state.last_soc is not None:
            item["endSoc"] = Decimal(str(state.last_soc))
        if soc_delta is not None:
            item["socDelta"] = Decimal(str(soc_delta))
        events.put_item(Item=item,
            ConditionExpression="attribute_not_exists(eventId)")
    except ClientError as error:
        if error.response.get("Error", {}).get("Code") == "ConditionalCheckFailedException":
            return 0
        raise
    if preference.get("chargingSummaryEmailEnabled") is not True:
        return 0
    name = str(preference.get("vehicleName") or vehicle_id)[:40]
    subject, text = charging_summary_email(
        preference, vehicle_id, name, state, duration, reason,
        capacity_kwh=capacity_kwh, coverage_percent=coverage,
        soc_estimate_kwh=soc_estimate,
    )
    result = sns.publish(TopicArn=email_topic_arn,
        Subject=subject[:100], Message=text,
        MessageAttributes={"recipientKey": {"DataType": "String", "StringValue": str(preference["recipientKey"])}})
    record_deliveries(identifier, [{"channel": "EMAIL", "messageId": result["MessageId"]}])
    return 1


def send_charging_summaries(vehicle_id, state, ended_at, reason):
    capacity_kwh = _charging_capacity_kwh(vehicle_id)
    return sum(dispatch_charging_summary(
               pref, vehicle_id, state, ended_at, reason, capacity_kwh)
               for pref in list_preferences(vehicle_id, None)
               if pref.get("emailEnabled") is True and (
                   pref.get("chargingSummaryEmailEnabled") is True
                   or pref.get("dailySummaryEmailEnabled") is True
               ))


def validate_charging_summary(message, now_ms):
    vehicle_id, session_id = str(message.get("vehicleId", "")), str(message.get("sessionId", ""))
    candidate_at = int(message.get("candidateAt", 0))
    for _ in range(4):
        item = sessions.get_item(Key={"vehicleId": vehicle_id}, ConsistentRead=True).get("Item", {})
        version = int(item.get("version", 0))
        state = _summary_state(item)
        if not charging_summary_due(state, session_id, candidate_at, now_ms):
            return 0
        completed = complete_charging_summary(
            state, candidate_at,
            capacity_kwh=_charging_capacity_kwh(vehicle_id),
        )
        updated = dict(item)
        updated.update({"vehicleId": vehicle_id, "version": version + 1,
                        "chargingSummary": _ddb(asdict(completed)),
                        "updatedAt": int(time.time() * 1000)})
        try:
            sessions.put_item(
                Item=updated,
                ConditionExpression="#version=:version",
                ExpressionAttributeNames={"#version": "version"},
                ExpressionAttributeValues={":version": version},
            )
            return send_charging_summaries(vehicle_id, state, candidate_at, "timeout")
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
                raise
    raise RuntimeError("charging summary completion contention")


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
    deliveries = []
    if preference.get("emailEnabled"):
        subject, email_text = charging_stop_email(
            preference, vehicle_id, vehicle_name, state.previous_soc, threshold
        )
        result = sns.publish(
            TopicArn=email_topic_arn,
            Subject=subject[:100], Message=email_text,
            MessageAttributes={
                "recipientKey": {
                    "DataType": "String",
                    "StringValue": str(preference["recipientKey"]),
                }
            },
        )
        deliveries.append({"channel": "EMAIL", "messageId": result["MessageId"]})
    sms_text = sms_charging_stop(
        preference, vehicle_id, state.previous_soc, threshold
    )
    sms_result = sms_delivery(preference, vehicle_id, sms_text)
    if sms_result:
        deliveries.append(sms_result)
    record_deliveries(identifier, deliveries)
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
    for preference in list_preferences(vehicle_id, None):
        if not (preference.get("chargingStopEmailEnabled") is True or preference.get("smsEnabled") is True):
            continue
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
                "durationMinutes": summary.duration_minutes,
                "energyDrawnKwh": Decimal(str(summary.energy_drawn_kwh)),
                "energyRegenKwh": Decimal(str(summary.energy_regen_kwh)),
                "energyNetKwh": Decimal(str(summary.energy_net_kwh)),
                "energySource": summary.energy_source,
                "completionTrigger": summary.completion_trigger,
                "receivedAt": summary.ended_at, "createdAt": now * 1000,
                "expiresAt": now + event_retention_days * 86400,
                "status": "RECORDED",
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


def _scan_all(table):
    result = []
    scan = {}
    while True:
        page = table.scan(**scan)
        result.extend(page.get("Items", []))
        key = page.get("LastEvaluatedKey")
        if not key:
            return result
        scan["ExclusiveStartKey"] = key


def _daily_event_id(user_sub, vehicle_id, report_date):
    material = f"{user_sub}|{vehicle_id}|daily-summary|{report_date}".encode("utf-8")
    return hashlib.sha256(material).hexdigest()


def _daily_activity_in_progress(vehicle_id, now_ms):
    session_item = sessions.get_item(
        Key={"vehicleId": vehicle_id}, ConsistentRead=True
    ).get("Item", {})
    charging = _summary_state(session_item)
    charging_active = charging.active and (
        charging.is_charging is True or charging.stop_candidate_at == 0
    )
    journey_item = sessions.get_item(
        Key={"vehicleId": _journey_session_id(vehicle_id)}, ConsistentRead=True
    ).get("Item", {})
    if not journey_item:
        journey_item = session_item
    journey = _journey_state(journey_item)
    journey_active = bool(journey.active_id)
    return (
        charging_active or journey_active
        or daily_recent_movement(journey.last_observed_moving_at, now_ms)
    )


def _reserve_daily_event(preference, report_date, now, status, summary):
    identifier = _daily_event_id(
        preference["userSub"], preference["vehicleId"], report_date
    )
    try:
        events.put_item(
            Item={
                "eventId": identifier,
                "eventType": "DAILY_SUMMARY",
                "userSub": preference["userSub"],
                "vehicleId": preference["vehicleId"],
                "reportDate": report_date,
                "createdAt": now * 1000,
                "expiresAt": now + event_retention_days * 86400,
                "status": status,
                "journeyCount": summary["journeyCount"],
                "chargingCount": summary["chargingCount"],
                "distanceKm": Decimal(str(summary["distanceKm"])),
                "journeyDurationMinutes": summary["journeyDurationMinutes"],
                "energyNetKwh": Decimal(str(summary["energyNetKwh"])),
                "chargingDurationMinutes": summary["chargingDurationMinutes"],
                "energyChargedKwh": Decimal(str(summary["energyChargedKwh"])),
            },
            ConditionExpression="attribute_not_exists(eventId)",
        )
        return identifier
    except ClientError as error:
        if error.response.get("Error", {}).get("Code") == "ConditionalCheckFailedException":
            return None
        raise


def _daily_text(preference, report_date, summary, ongoing):
    return daily_summary_email(preference, report_date, summary, ongoing)[1]


def send_daily_summaries(now_ms):
    report_date, start_ms, end_ms, force_send = report_window(now_ms)
    all_events = _scan_all(events)
    now = int(time.time())
    result = {"accepted": True, "daily": True, "date": report_date,
              "evaluated": 0, "deferred": 0, "deliveries": 0}
    for preference in _scan_all(preferences):
        if not (
            preference.get("dailySummaryEmailEnabled") is True
            and preference.get("emailEnabled") is True
        ):
            continue
        result["evaluated"] += 1
        matching = [item for item in all_events if (
            item.get("userSub") == preference.get("userSub")
            and item.get("vehicleId") == preference.get("vehicleId")
        )]
        summary = aggregate_daily(matching, start_ms, end_ms)
        ongoing = _daily_activity_in_progress(preference["vehicleId"], now_ms)
        if ongoing and not force_send:
            result["deferred"] += 1
            continue
        if not daily_has_activity(summary):
            continue
        identifier = _reserve_daily_event(
            preference, report_date, now, "RECORDED", summary
        )
        if not identifier:
            continue
        subject, text = daily_summary_email(
            preference, report_date, summary, ongoing
        )
        message = sns.publish(
            TopicArn=email_topic_arn,
            Subject=subject[:100], Message=text,
            MessageAttributes={"recipientKey": {
                "DataType": "String",
                "StringValue": str(preference["recipientKey"]),
            }},
        )
        record_deliveries(identifier, [{"channel": "EMAIL", "messageId": message["MessageId"]}])
        result["deliveries"] += 1
    return result


def dispatch_journey(preference, identifier, vehicle_id, summary):
    vehicle_name = str(preference.get("vehicleName") or vehicle_id)[:40]
    subject, text = journey_summary_email(
        preference, vehicle_id, vehicle_name, summary
    )
    deliveries = []
    if preference.get("emailEnabled"):
        result = sns.publish(
            TopicArn=email_topic_arn,
            Subject=subject[:100],
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


def finalize_journey(vehicle_id, now_ms, finalize_stable_stop=False):
    """Atomically close one due journey and emit each user's email once."""
    for attempt in range(JOURNEY_UPDATE_ATTEMPTS):
        current = _read_journey_session(vehicle_id)
        state = apply_inactivity_timeout(_journey_state(current), now_ms)
        summary, reason = summarize_journey(
            state, now_ms, finalize_stable_stop=finalize_stable_stop
        )
        if reason == "not_due":
            return 0, reason
        try:
            aggregate = _drive_since_charge_state(current)
            measurement = measurement_from_journey(state)
            if measurement:
                aggregate = accumulate_drive_since_charge(
                    aggregate, measurement, _charge_reference_at(vehicle_id)
                )
            _put_journey(
                vehicle_id, current,
                clear_journey(state, reason=reason, completed_at=now_ms),
                aggregate,
            )
            break
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
                raise
            _journey_retry_delay(attempt)
    else:
        raise RuntimeError("journey finalization contention")
    if not summary:
        return 0, reason
    dispatched = 0
    for preference in list_preferences(vehicle_id, None):
        if not (
            preference.get("emailEnabled") is True
            and (preference.get("journeyEmailEnabled") is True
                 or preference.get("dailySummaryEmailEnabled") is True)
        ):
            continue
        identifier = reserve_journey_event(preference, vehicle_id, summary)
        if identifier and preference.get("journeyEmailEnabled") is True:
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
            journey_vehicle_id = item.get("journeyVehicleId")
            if journey_vehicle_id and state.active_id:
                count, _ = finalize_journey(journey_vehicle_id, now_ms)
                dispatched += count
                evaluated += 1
            elif state.active_id and not str(item.get("vehicleId", "")).startswith(
                JOURNEY_SESSION_PREFIX
            ):
                isolated = sessions.get_item(
                    Key={"vehicleId": _journey_session_id(item["vehicleId"])},
                    ConsistentRead=True,
                ).get("Item")
                if not isolated:
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
    if event.get("type") == "daily_summary":
        return send_daily_summaries(int(time.time() * 1000))
    if event.get("Records") and all(
        record.get("eventSource") == "aws:sqs" for record in event["Records"]
    ):
        dispatched = 0
        for record in event["Records"]:
            message = json.loads(record.get("body") or "{}")
            dispatched += (validate_charging_summary(message, int(time.time() * 1000))
                           if message.get("type") == "charging_summary" else
                           validate_charging_stop(message, int(time.time() * 1000)))
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
    value = _decode(
        event.get("payloadBase64"),
        allow_plain_counter_id=(suffix == "journey/energy_counter_id"),
    )
    dispatched = 0
    if suffix in CHARGING_SUFFIXES:
        before, after = update_session(vehicle_id, suffix, value, received_at)
        if (
            suffix == "charging/is_charging"
            and value is False
            and after.stop_candidate_at == received_at
        ):
            enqueue_charging_stop(vehicle_id, after)
    summary_before, summary_after = update_charging_states(
        vehicle_id, suffix, value, received_at
    )
    if suffix == "charging/is_charging" and value is False and summary_after.stop_candidate_at == received_at:
        enqueue_charging_summary(vehicle_id, summary_after)
    if suffix == "charging/plugged" and value is False and summary_before.active:
        dispatched += send_charging_summaries(vehicle_id, summary_before, received_at, "unplugged")
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
        state, resumed_after_stable_stop = update_journey(
            vehicle_id, suffix, value, received_at
        )
        if resumed_after_stable_stop:
            completed, _ = finalize_journey(
                vehicle_id, received_at, finalize_stable_stop=True
            )
            dispatched += completed
            state, _ = update_journey(vehicle_id, suffix, value, received_at)
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
