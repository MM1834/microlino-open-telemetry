"""Authorized SMS destination verification and approval-status API."""

import hashlib
import json
import os
import re
import time

import boto3
from botocore.exceptions import ClientError


dynamodb = boto3.resource("dynamodb")
preferences = dynamodb.Table(os.environ["PREFERENCE_TABLE_NAME"])
access = dynamodb.Table(os.environ["ACCESS_TABLE_NAME"])
approvals = dynamodb.Table(os.environ["SMS_APPROVAL_TABLE_NAME"])
destinations = dynamodb.Table(os.environ["SMS_DESTINATION_TABLE_NAME"])
sms = boto3.client("pinpoint-sms-voice-v2")
configuration_set = os.environ["SMS_CONFIGURATION_SET"]
sender_arns = {country: os.environ[f"SMS_SENDER_ARN_{country}"] for country in ("CH", "DE")}
read_only_vehicle_ids = {
    value.strip() for value in os.environ.get("READ_ONLY_VEHICLE_IDS", "").split(",")
    if value.strip()
}

PHONE = re.compile(r"^\+(41|49)[1-9][0-9]{7,11}$")
CODE = re.compile(r"^[0-9]{4,10}$")
RESEND_SECONDS = 60


def response(status, body):
    return {
        "statusCode": status,
        "headers": {"content-type": "application/json", "cache-control": "no-store"},
        "body": json.dumps(body, separators=(",", ":")),
    }


def caller_sub(event):
    claims = event.get("requestContext", {}).get("authorizer", {}).get("jwt", {}).get("claims", {})
    return str(claims.get("sub", "")).strip()


def authorized(user_sub, vehicle_id):
    item = access.get_item(
        Key={"userSub": user_sub, "vehicleId": vehicle_id}, ConsistentRead=True
    ).get("Item", {})
    return item.get("status") == "ACTIVE"


def normalize_phone(value):
    phone = re.sub(r"[\s()-]", "", str(value or "").strip())
    return phone if PHONE.fullmatch(phone) else ""


def fingerprint(phone):
    return hashlib.sha256(phone.encode()).hexdigest()


def country(phone):
    return "CH" if phone.startswith("+41") else "DE"


def originator(phone):
    return sender_arns[country(phone)]


def approval_active(item, destination_fingerprint, now):
    return bool(
        item
        and item.get("status") == "ACTIVE"
        and item.get("destinationFingerprint") == destination_fingerprint
        and item.get("isoCountryCode") in ("CH", "DE")
        and item.get("originator") == "MOT"
        and int(item.get("expiresAt", 0)) > now
    )


def current_status(user_sub, vehicle_id):
    now = int(time.time())
    key = {"vehicleId": vehicle_id, "userSub": user_sub}
    preference = preferences.get_item(Key=key, ConsistentRead=True).get("Item", {})
    phone = normalize_phone(preference.get("phoneE164"))
    destination_fingerprint = fingerprint(phone) if phone else ""
    destination = destinations.get_item(
        Key={"destinationFingerprint": destination_fingerprint}, ConsistentRead=True
    ).get("Item", {}) if destination_fingerprint else {}
    approval = approvals.get_item(Key=key, ConsistentRead=True).get("Item", {})
    verified = destination.get("status") == "VERIFIED"
    approved = approval_active(approval, destination_fingerprint, now)
    return {
        "phoneE164": phone,
        "verificationStatus": "VERIFIED" if verified else (
            "PENDING" if destination.get("status") == "PENDING" else "UNVERIFIED"
        ),
        "smsApproved": approved,
        "smsEnabled": preference.get("smsEnabled") is True,
        "smsReady": verified and approved,
    }


def request_verification(user_sub, vehicle_id, body):
    phone = normalize_phone(body.get("phoneE164"))
    if not phone:
        return response(400, {"error": "invalid_phone"})
    now = int(time.time())
    destination_fingerprint = fingerprint(phone)
    destination = destinations.get_item(
        Key={"destinationFingerprint": destination_fingerprint}, ConsistentRead=True
    ).get("Item", {})
    preference_key = {"vehicleId": vehicle_id, "userSub": user_sub}
    if destination.get("status") == "VERIFIED":
        preferences.update_item(
            Key=preference_key,
            UpdateExpression="SET phoneE164=:phone, smsConfirmed=:confirmed, smsEnabled=:disabled, updatedAt=:updated",
            ExpressionAttributeValues={
                ":phone": phone, ":confirmed": True, ":disabled": False,
                ":updated": now * 1000,
            },
        )
        return response(200, current_status(user_sub, vehicle_id))
    if int(destination.get("lastSentAt", 0)) > now - RESEND_SECONDS:
        return response(429, {"error": "verification_rate_limited"})

    destination_id = str(destination.get("verifiedDestinationNumberId", ""))
    if not destination_id:
        created = sms.create_verified_destination_number(
            DestinationPhoneNumber=phone,
            ClientToken=f"mot-{destination_fingerprint[:32]}",
        )
        destination_id = created["VerifiedDestinationNumberId"]
        destinations.put_item(Item={
            "destinationFingerprint": destination_fingerprint,
            "verifiedDestinationNumberId": destination_id,
            "status": "PENDING", "createdAt": now, "expiresAt": now + 86400,
        })
    sms.send_destination_number_verification_code(
        VerifiedDestinationNumberId=destination_id,
        VerificationChannel="TEXT", LanguageCode="DE_DE",
        OriginationIdentity=originator(phone),
        ConfigurationSetName=configuration_set,
    )
    destinations.update_item(
        Key={"destinationFingerprint": destination_fingerprint},
        UpdateExpression="SET lastSentAt=:now",
        ExpressionAttributeValues={":now": now},
    )
    preferences.update_item(
        Key=preference_key,
        UpdateExpression="SET phoneE164=:phone, smsConfirmed=:pending, smsEnabled=:disabled, updatedAt=:updated",
        ExpressionAttributeValues={
            ":phone": phone, ":pending": False, ":disabled": False,
            ":updated": now * 1000,
        },
    )
    return response(200, current_status(user_sub, vehicle_id))


def confirm_verification(user_sub, vehicle_id, body):
    code = str(body.get("verificationCode", "")).strip()
    if not CODE.fullmatch(code):
        return response(400, {"error": "invalid_verification_code"})
    key = {"vehicleId": vehicle_id, "userSub": user_sub}
    preference = preferences.get_item(Key=key, ConsistentRead=True).get("Item", {})
    phone = normalize_phone(preference.get("phoneE164"))
    if not phone:
        return response(409, {"error": "verification_not_requested"})
    destination_fingerprint = fingerprint(phone)
    destination = destinations.get_item(
        Key={"destinationFingerprint": destination_fingerprint}, ConsistentRead=True
    ).get("Item", {})
    destination_id = str(destination.get("verifiedDestinationNumberId", ""))
    if not destination_id:
        return response(409, {"error": "verification_not_requested"})
    try:
        sms.verify_destination_number(
            VerifiedDestinationNumberId=destination_id, VerificationCode=code
        )
    except ClientError as error:
        error_code = error.response.get("Error", {}).get("Code", "")
        if error_code in ("ValidationException", "ConflictException"):
            return response(400, {"error": "verification_failed"})
        raise
    now = int(time.time())
    destinations.update_item(
        Key={"destinationFingerprint": destination_fingerprint},
        UpdateExpression="SET #status=:verified, verifiedAt=:now REMOVE expiresAt",
        ExpressionAttributeNames={"#status": "status"},
        ExpressionAttributeValues={":verified": "VERIFIED", ":now": now},
    )
    preferences.update_item(
        Key=key,
        UpdateExpression="SET smsConfirmed=:confirmed, updatedAt=:updated",
        ConditionExpression="phoneE164=:phone",
        ExpressionAttributeValues={
            ":confirmed": True, ":updated": now * 1000, ":phone": phone,
        },
    )
    return response(200, current_status(user_sub, vehicle_id))


def handler(event, context):
    del context
    method = event.get("requestContext", {}).get("http", {}).get("method", "GET")
    user_sub = caller_sub(event)
    vehicle_id = str((event.get("pathParameters") or {}).get("vehicleId", "")).strip()
    if not user_sub:
        return response(401, {"error": "unauthorized"})
    if not vehicle_id or not authorized(user_sub, vehicle_id):
        return response(404, {"error": "vehicle_not_found"})
    if vehicle_id in read_only_vehicle_ids:
        return response(403, {"error": "notifications_read_only"})
    route = str(event.get("routeKey", ""))
    try:
        body = json.loads(event.get("body") or "{}")
    except json.JSONDecodeError:
        return response(400, {"error": "invalid_request"})
    if method == "GET":
        return response(200, current_status(user_sub, vehicle_id))
    if route.startswith("POST ") and route.endswith("/request"):
        return request_verification(user_sub, vehicle_id, body)
    if route.startswith("POST ") and route.endswith("/confirm"):
        return confirm_verification(user_sub, vehicle_id, body)
    return response(405, {"error": "method_not_allowed"})
