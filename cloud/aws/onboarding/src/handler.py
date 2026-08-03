"""ONB-001.B2 claim issue and atomic consumption handlers."""

from __future__ import annotations

import base64
import hashlib
import json
import os
import re
import secrets
import time

import boto3
from boto3.dynamodb.types import TypeSerializer
from boto3.dynamodb.conditions import Key
from botocore.exceptions import ClientError


CLAIM_TTL_SECONDS = int(os.environ.get("CLAIM_TTL_SECONDS", "86400"))
MAX_ATTEMPTS = int(os.environ.get("MAX_ATTEMPTS", "5"))
ADMIN_GROUP = os.environ.get("ADMIN_GROUP", "mot-beta-admins")
AUDIT_TTL_SECONDS = int(os.environ.get("AUDIT_RETENTION_DAYS", "0")) * 86400
CLAIMS_TABLE = os.environ.get("CLAIMS_TABLE", "")
OWNERSHIP_TABLE = os.environ.get("OWNERSHIP_TABLE", "")
ACCESS_TABLE = os.environ.get("ACCESS_TABLE", "")
AUDIT_TABLE = os.environ.get("AUDIT_TABLE", "")
STATE_TABLE = os.environ.get("STATE_TABLE", "")

dynamodb = boto3.resource("dynamodb")
ddb_client = boto3.client("dynamodb")
serializer = TypeSerializer()
VEHICLE_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,63}$")
CLAIM_PART_RE = re.compile(r"^[A-Za-z0-9_-]{22,64}$")


def _json(status: int, body: dict) -> dict:
    return {
        "statusCode": status,
        "headers": {"content-type": "application/json", "cache-control": "no-store"},
        "body": json.dumps(body, separators=(",", ":")),
    }


def _claims(event: dict) -> dict:
    return (
        event.get("requestContext", {})
        .get("authorizer", {})
        .get("jwt", {})
        .get("claims", {})
    )


def _groups(claims: dict) -> set[str]:
    value = claims.get("cognito:groups", [])
    if isinstance(value, str):
        value = [item.strip() for item in value.strip("[]").split(",")]
    return {str(item) for item in value if str(item)}


def _body(event: dict) -> dict:
    try:
        value = json.loads(event.get("body") or "{}")
    except (TypeError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def _hash(claim_id: str, salt: str, proof: str) -> str:
    material = f"mot:onboarding-claim:v1\0{claim_id}\0{salt}\0{proof}".encode()
    digest = base64.urlsafe_b64encode(hashlib.sha256(material).digest()).decode().rstrip("=")
    return f"sha256:{digest}"


def _ddb(item: dict) -> dict:
    return {key: serializer.serialize(value) for key, value in item.items()}


def _audit(entity_id: str, event_type: str, now: int, **values) -> dict:
    item = {
        "schemaVersion": 1,
        "entityId": entity_id,
        "eventKey": f"{now}#{secrets.token_urlsafe(18)}",
        "eventType": event_type,
        "occurredAt": now,
        "actorType": values.pop("actorType", "USER"),
        "result": values.pop("result", "SUCCEEDED"),
        **values,
    }
    if AUDIT_TTL_SECONDS > 0:
        item["ttl"] = now + AUDIT_TTL_SECONDS
    return item


def issue_claim(event: dict, claims: dict) -> dict:
    if ADMIN_GROUP not in _groups(claims):
        return _json(403, {"error": "forbidden"})
    body = _body(event)
    vehicle_id = str(body.get("vehicleId", ""))
    if not VEHICLE_RE.fullmatch(vehicle_id):
        return _json(400, {"error": "invalid_request"})

    state = dynamodb.Table(STATE_TABLE).query(
        KeyConditionExpression=Key("vehicleId").eq(vehicle_id),
        Limit=1,
        ConsistentRead=True,
    )
    if not state.get("Items"):
        return _json(404, {"error": "vehicle_not_provisionable"})
    owner = dynamodb.Table(OWNERSHIP_TABLE).get_item(
        Key={"vehicleId": vehicle_id}, ConsistentRead=True
    ).get("Item")
    if owner and owner.get("status") in {"ACTIVE", "TRANSFER_PENDING"}:
        return _json(409, {"error": "vehicle_unavailable"})
    legacy_access = dynamodb.Table(ACCESS_TABLE).scan(
        FilterExpression="vehicleId = :vehicle AND #status = :active",
        ExpressionAttributeNames={"#status": "status"},
        ExpressionAttributeValues={":vehicle": vehicle_id, ":active": "ACTIVE"},
        ProjectionExpression="userSub",
        ConsistentRead=True,
    )
    if legacy_access.get("LastEvaluatedKey") or legacy_access.get("Items"):
        return _json(409, {"error": "vehicle_unavailable"})

    now = int(time.time())
    claim_id = secrets.token_urlsafe(18)
    proof = secrets.token_urlsafe(24)
    salt = secrets.token_urlsafe(18)
    item = {
        "schemaVersion": 1,
        "claimId": claim_id,
        "vehicleId": vehicle_id,
        "proofHash": _hash(claim_id, salt, proof),
        "proofSalt": salt,
        "status": "ISSUED",
        "issuedAt": now,
        "expiresAt": now + CLAIM_TTL_SECONDS,
        "failedAttempts": 0,
        "maxAttempts": MAX_ATTEMPTS,
        "issuedBySub": claims["sub"],
        "ttl": now + CLAIM_TTL_SECONDS,
    }
    audit = _audit(
        claim_id, "CLAIM_ISSUED", now, actorType="ADMIN",
        actorSub=claims["sub"], vehicleId=vehicle_id,
    )
    ddb_client.transact_write_items(TransactItems=[
        {"Put": {"TableName": CLAIMS_TABLE, "Item": _ddb(item),
                  "ConditionExpression": "attribute_not_exists(claimId)"}},
        {"Put": {"TableName": AUDIT_TABLE, "Item": _ddb(audit),
                  "ConditionExpression": "attribute_not_exists(entityId) AND attribute_not_exists(eventKey)"}},
    ])
    return _json(201, {
        "claim": f"{claim_id}.{proof}",
        "expiresAt": item["expiresAt"],
        "vehicleId": vehicle_id,
    })


def consume_claim(event: dict, claims: dict) -> dict:
    user_sub = str(claims.get("sub", ""))
    supplied = str(_body(event).get("claim", ""))
    if not user_sub or supplied.count(".") != 1:
        return _json(409, {"error": "claim_invalid_or_unavailable"})
    claim_id, proof = supplied.split(".", 1)
    if not CLAIM_PART_RE.fullmatch(claim_id) or not CLAIM_PART_RE.fullmatch(proof):
        return _json(409, {"error": "claim_invalid_or_unavailable"})
    table = dynamodb.Table(CLAIMS_TABLE)
    item = table.get_item(Key={"claimId": claim_id}, ConsistentRead=True).get("Item")
    now = int(time.time())
    valid = bool(
        item and item.get("status") == "ISSUED"
        and int(item.get("expiresAt", 0)) > now
        and int(item.get("failedAttempts", 0)) < int(item.get("maxAttempts", 0))
        and secrets.compare_digest(
            str(item.get("proofHash", "")),
            _hash(claim_id, str(item.get("proofSalt", "")), proof),
        )
    )
    if not valid:
        if item and item.get("status") == "ISSUED" and int(item.get("expiresAt", 0)) > now:
            audit = _audit(
                claim_id, "CLAIM_FAILED", now, actorSub=user_sub,
                vehicleId=item.get("vehicleId"), claimId=claim_id,
                result="DENIED", reasonCode="INVALID_OR_UNAVAILABLE",
            )
            try:
                ddb_client.transact_write_items(TransactItems=[
                    {"Update": {
                        "TableName": CLAIMS_TABLE,
                        "Key": _ddb({"claimId": claim_id}),
                        "UpdateExpression": "SET failedAttempts = failedAttempts + :one",
                        "ConditionExpression": "#s = :issued AND failedAttempts < maxAttempts",
                        "ExpressionAttributeNames": {"#s": "status"},
                        "ExpressionAttributeValues": _ddb({":one": 1, ":issued": "ISSUED"}),
                    }},
                    {"Put": {"TableName": AUDIT_TABLE, "Item": _ddb(audit),
                             "ConditionExpression": "attribute_not_exists(entityId) AND attribute_not_exists(eventKey)"}},
                ])
            except ClientError:
                pass
        return _json(409, {"error": "claim_invalid_or_unavailable"})

    vehicle_id = item["vehicleId"]
    ownership = {
        "schemaVersion": 1, "vehicleId": vehicle_id, "ownerUserSub": user_sub,
        "status": "ACTIVE", "createdAt": now, "updatedAt": now, "version": 1,
        "sourceClaimId": claim_id,
    }
    access = {
        "userSub": user_sub, "vehicleId": vehicle_id, "status": "ACTIVE",
        "role": "OWNER", "createdAt": str(now), "updatedAt": str(now),
        "source": "onb-001-b2-claim",
    }
    audit = _audit(
        claim_id, "CLAIM_CONSUMED", now, actorSub=user_sub,
        vehicleId=vehicle_id, claimId=claim_id,
    )
    try:
        ddb_client.transact_write_items(TransactItems=[
            {"Update": {
                "TableName": CLAIMS_TABLE, "Key": _ddb({"claimId": claim_id}),
                "UpdateExpression": "SET #s=:consumed, consumedAt=:now, consumedBySub=:sub",
                "ConditionExpression": "#s=:issued AND expiresAt>:now AND failedAttempts<maxAttempts AND proofHash=:hash",
                "ExpressionAttributeNames": {"#s": "status"},
                "ExpressionAttributeValues": _ddb({
                    ":consumed": "CONSUMED", ":issued": "ISSUED", ":now": now,
                    ":sub": user_sub, ":hash": item["proofHash"],
                }),
            }},
            {"Put": {"TableName": OWNERSHIP_TABLE, "Item": _ddb(ownership),
                     "ConditionExpression": "attribute_not_exists(vehicleId)"}},
            {"Put": {"TableName": ACCESS_TABLE, "Item": _ddb(access),
                     "ConditionExpression": "attribute_not_exists(userSub) AND attribute_not_exists(vehicleId)"}},
            {"Put": {"TableName": AUDIT_TABLE, "Item": _ddb(audit),
                     "ConditionExpression": "attribute_not_exists(entityId) AND attribute_not_exists(eventKey)"}},
        ])
    except ClientError:
        return _json(409, {"error": "claim_invalid_or_unavailable"})
    return _json(200, {"status": "claimed", "vehicleId": vehicle_id})


def handler(event: dict, _context) -> dict:
    claims = _claims(event)
    if not claims.get("sub"):
        return _json(401, {"error": "unauthorized"})
    route = event.get("routeKey", "")
    if route == "POST /api/onboarding/claims":
        return issue_claim(event, claims)
    if route == "POST /api/onboarding/claim":
        return consume_claim(event, claims)
    return _json(404, {"error": "not_found"})
