"""Authorized NTF-001 per-user/per-vehicle preference API."""

import hashlib
import json
import os
import re
import time

import boto3


dynamodb = boto3.resource("dynamodb")
preferences = dynamodb.Table(os.environ["PREFERENCE_TABLE_NAME"])
access = dynamodb.Table(os.environ["ACCESS_TABLE_NAME"])
sns = boto3.client("sns")
email_topic_arn = os.environ["EMAIL_TOPIC_ARN"]

EMAIL = re.compile(r"^[^\s@]+@[^\s@]+\.[^\s@]+$")
PHONE = re.compile(r"^\+[1-9][0-9]{7,14}$")


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


def recipient_key(user_sub, vehicle_id, email):
    return hashlib.sha256(
        f"{user_sub}|{vehicle_id}|{email.lower()}".encode()
    ).hexdigest()


def public(item):
    if not item:
        return {
            "enabled": False, "threshold": 80,
            "emailEnabled": False, "smsEnabled": False,
            "emailConfirmed": False, "smsConfirmed": False,
        }
    result = {
        key: item.get(key) for key in (
            "vehicleId", "enabled", "threshold", "emailEnabled", "smsEnabled",
            "email", "phoneE164", "emailConfirmed", "smsConfirmed", "updatedAt"
        )
    }
    result["threshold"] = int(result.get("threshold") or 80)
    result["updatedAt"] = int(result.get("updatedAt") or 0)
    return result


def handler(event, context):
    method = event.get("requestContext", {}).get("http", {}).get("method", "GET")
    user_sub = caller_sub(event)
    vehicle_id = str((event.get("pathParameters") or {}).get("vehicleId", "")).strip()
    if not user_sub:
        return response(401, {"error": "unauthorized"})
    if not vehicle_id or not authorized(user_sub, vehicle_id):
        return response(404, {"error": "vehicle_not_found"})
    key = {"vehicleId": vehicle_id, "userSub": user_sub}
    if method == "GET":
        return response(200, public(preferences.get_item(Key=key, ConsistentRead=True).get("Item")))
    if method != "PUT":
        return response(405, {"error": "method_not_allowed"})
    try:
        body = json.loads(event.get("body") or "{}")
        threshold = int(body.get("threshold", 80))
    except (TypeError, ValueError, json.JSONDecodeError):
        return response(400, {"error": "invalid_request"})
    if threshold < 50 or threshold > 100:
        return response(400, {"error": "invalid_threshold"})
    previous = preferences.get_item(Key=key, ConsistentRead=True).get("Item", {})
    email = str(body.get("email", previous.get("email", ""))).strip().lower()
    phone = re.sub(
        r"[\s()-]", "",
        str(body.get("phoneE164", previous.get("phoneE164", ""))).strip()
    )
    email_enabled = body.get("emailEnabled") is True
    sms_enabled = body.get("smsEnabled") is True
    if email_enabled and not EMAIL.fullmatch(email):
        return response(400, {"error": "invalid_email"})
    if sms_enabled and not PHONE.fullmatch(phone):
        return response(400, {"error": "invalid_phone"})

    item = {
        **key,
        "enabled": body.get("enabled") is True,
        "threshold": threshold,
        "emailEnabled": email_enabled,
        "smsEnabled": sms_enabled,
        "email": email,
        "phoneE164": phone,
        "emailConfirmed": previous.get("email") == email and previous.get("emailConfirmed") is True,
        "smsConfirmed": previous.get("phoneE164") == phone and previous.get("smsConfirmed") is True,
        "recipientKey": recipient_key(user_sub, vehicle_id, email) if email else "disabled",
        "updatedAt": int(time.time() * 1000),
    }
    if email_enabled and previous.get("email") != email:
        subscription = sns.subscribe(
            TopicArn=email_topic_arn,
            Protocol="email",
            Endpoint=email,
            Attributes={"FilterPolicy": json.dumps({"recipientKey": [item["recipientKey"]]})},
            ReturnSubscriptionArn=True,
        )
        item["emailSubscriptionArn"] = subscription.get("SubscriptionArn", "PendingConfirmation")
    elif previous.get("emailSubscriptionArn"):
        item["emailSubscriptionArn"] = previous["emailSubscriptionArn"]
    preferences.put_item(Item=item)
    return response(200, public(item))
