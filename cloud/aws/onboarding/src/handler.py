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
CLAIM_READ_ONLY_VEHICLE_IDS = {
    item.strip()
    for item in os.environ.get("CLAIM_READ_ONLY_VEHICLE_IDS", "").split(",")
    if item.strip()
}
FIRMWARE_GRANTS_TABLE = os.environ.get("FIRMWARE_GRANTS_TABLE", "")
FIRMWARE_BUCKET = os.environ.get("FIRMWARE_BUCKET", "")
FIRMWARE_ARTIFACT_KEY = os.environ.get("FIRMWARE_ARTIFACT_KEY", "")
FIRMWARE_TARGET = os.environ.get("FIRMWARE_TARGET", "nanoesp32c6-n16")
FIRMWARE_FLASH_SIZE_BYTES = int(os.environ.get("FIRMWARE_FLASH_SIZE_BYTES", str(16 * 1024 * 1024)))
FIRMWARE_VERSION = os.environ.get("FIRMWARE_VERSION", "")
FIRMWARE_SHA256 = os.environ.get("FIRMWARE_SHA256", "")
FIRMWARE_SIZE = int(os.environ.get("FIRMWARE_SIZE", "0"))
FIRMWARE_SECONDARY_ARTIFACT_KEY = os.environ.get("FIRMWARE_SECONDARY_ARTIFACT_KEY", "")
FIRMWARE_SECONDARY_TARGET = os.environ.get("FIRMWARE_SECONDARY_TARGET", "")
FIRMWARE_SECONDARY_FLASH_SIZE_BYTES = int(os.environ.get("FIRMWARE_SECONDARY_FLASH_SIZE_BYTES", "0"))
FIRMWARE_SECONDARY_VERSION = os.environ.get("FIRMWARE_SECONDARY_VERSION", "")
FIRMWARE_SECONDARY_SHA256 = os.environ.get("FIRMWARE_SECONDARY_SHA256", "")
FIRMWARE_SECONDARY_SIZE = int(os.environ.get("FIRMWARE_SECONDARY_SIZE", "0"))
FIRMWARE_URL_TTL_SECONDS = int(os.environ.get("FIRMWARE_URL_TTL_SECONDS", "300"))
COGNITO_USER_POOL_ID = os.environ.get("COGNITO_USER_POOL_ID", "")
AWS_REGION = os.environ.get("AWS_REGION", "eu-north-1")

dynamodb = boto3.resource("dynamodb")
ddb_client = boto3.client("dynamodb")
s3_client = boto3.client(
    "s3",
    region_name=AWS_REGION,
    endpoint_url=f"https://s3.{AWS_REGION}.amazonaws.com",
)
cognito_client = boto3.client("cognito-idp")
serializer = TypeSerializer()
VEHICLE_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,63}$")
CLAIM_PART_RE = re.compile(r"^[A-Za-z0-9_-]{22,64}$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


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


def _claim_read_only(user_sub: str) -> bool:
    table = dynamodb.Table(ACCESS_TABLE)
    for vehicle_id in CLAIM_READ_ONLY_VEHICLE_IDS:
        item = table.get_item(
            Key={"userSub": user_sub, "vehicleId": vehicle_id},
            ConsistentRead=True,
        ).get("Item", {})
        if item.get("status") == "ACTIVE":
            return True
    return False


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


def _release(target: str, artifact_key: str, version: str, flash_size: int,
             size: int, sha256: str) -> dict | None:
    if not (
        FIRMWARE_BUCKET and target and artifact_key and version and flash_size > 0
        and size > 0 and SHA256_RE.fullmatch(sha256)
    ):
        return None
    return {
        "target": target,
        "version": version,
        "chipFamily": "ESP32-C6",
        "flashSizeBytes": flash_size,
        "offset": 0x10000,
        "size": size,
        "sha256": sha256,
        "factoryErase": False,
        "artifactKey": artifact_key,
    }


def _firmware_releases() -> dict[str, dict]:
    candidates = (
        _release(FIRMWARE_TARGET, FIRMWARE_ARTIFACT_KEY, FIRMWARE_VERSION,
                 FIRMWARE_FLASH_SIZE_BYTES, FIRMWARE_SIZE, FIRMWARE_SHA256),
        _release(FIRMWARE_SECONDARY_TARGET, FIRMWARE_SECONDARY_ARTIFACT_KEY,
                 FIRMWARE_SECONDARY_VERSION, FIRMWARE_SECONDARY_FLASH_SIZE_BYTES,
                 FIRMWARE_SECONDARY_SIZE, FIRMWARE_SECONDARY_SHA256),
    )
    return {release["target"]: release for release in candidates if release}


def _public_release(release: dict) -> dict:
    return {key: value for key, value in release.items() if key != "artifactKey"}


def _firmware_release(target: str | None = None) -> dict | None:
    releases = _firmware_releases()
    return releases.get(target or FIRMWARE_TARGET)


def _firmware_grant(user_sub: str, target: str) -> dict:
    if not FIRMWARE_GRANTS_TABLE:
        return {}
    return dynamodb.Table(FIRMWARE_GRANTS_TABLE).get_item(
        Key={"userSub": user_sub, "target": target},
        ConsistentRead=True,
    ).get("Item", {})


def _active_firmware_release(user_sub: str, now: int,
                             target: str | None = None) -> tuple[dict, dict] | tuple[None, None]:
    releases = _firmware_releases()
    targets = [target] if target else list(releases)
    matches = []
    for candidate in targets:
        release = releases.get(candidate)
        grant = _firmware_grant(user_sub, candidate) if release else {}
        if release and (
            grant.get("status") == "ACTIVE"
            and int(grant.get("expiresAt", 0)) > now
            and grant.get("version") == release["version"]
            and grant.get("sha256") == release["sha256"]
        ):
            matches.append((grant, release))
    return matches[0] if len(matches) == 1 else (None, None)


def _resolve_user_sub(username: str) -> str:
    if not username or len(username) > 320:
        return ""
    try:
        result = cognito_client.admin_get_user(
            UserPoolId=COGNITO_USER_POOL_ID, Username=username,
        )
    except ClientError:
        return ""
    attributes = {item["Name"]: item["Value"] for item in result.get("UserAttributes", [])}
    return str(attributes.get("sub", "")) if result.get("Enabled") is not False else ""


def firmware_grant(event: dict, claims: dict) -> dict:
    if ADMIN_GROUP not in _groups(claims):
        return _json(403, {"error": "forbidden"})
    body = _body(event)
    target = str(body.get("target", FIRMWARE_TARGET))
    release = _firmware_release(target)
    username = str(body.get("username", "")).strip()
    hours = body.get("expiresInHours", 48)
    if not release:
        return _json(409, {"error": "firmware_release_unavailable"})
    if not isinstance(hours, int) or isinstance(hours, bool) or not 1 <= hours <= 168:
        return _json(400, {"error": "invalid_request"})
    user_sub = _resolve_user_sub(username)
    if not user_sub:
        return _json(404, {"error": "user_not_found"})
    now = int(time.time())
    expires_at = now + hours * 3600
    item = {
        "schemaVersion": 1, "userSub": user_sub, "target": target,
        "status": "ACTIVE", "version": release["version"],
        "sha256": release["sha256"], "grantedAt": now,
        "grantedBySub": claims["sub"], "expiresAt": expires_at,
        "ttl": expires_at + 86400,
    }
    audit = _audit(
        f"firmware#{user_sub}", "FIRMWARE_GRANT_CREATED", now,
        actorType="ADMIN", actorSub=claims["sub"], target=target,
        version=release["version"], expiresAt=expires_at,
    )
    ddb_client.transact_write_items(TransactItems=[
        {"Put": {"TableName": FIRMWARE_GRANTS_TABLE, "Item": _ddb(item)}},
        {"Put": {"TableName": AUDIT_TABLE, "Item": _ddb(audit),
                 "ConditionExpression": "attribute_not_exists(entityId) AND attribute_not_exists(eventKey)"}},
    ])
    return _json(200, {
        "status": "ACTIVE", "target": target,
        "version": release["version"], "expiresAt": expires_at,
    })


def firmware_revoke(event: dict, claims: dict) -> dict:
    if ADMIN_GROUP not in _groups(claims):
        return _json(403, {"error": "forbidden"})
    body = _body(event)
    target = str(body.get("target", FIRMWARE_TARGET))
    if target not in _firmware_releases():
        return _json(409, {"error": "firmware_release_unavailable"})
    username = str(body.get("username", "")).strip()
    user_sub = _resolve_user_sub(username)
    if not user_sub:
        return _json(404, {"error": "user_not_found"})
    now = int(time.time())
    audit = _audit(
        f"firmware#{user_sub}", "FIRMWARE_GRANT_REVOKED", now,
        actorType="ADMIN", actorSub=claims["sub"], target=target,
    )
    try:
        ddb_client.transact_write_items(TransactItems=[
            {"Update": {
                "TableName": FIRMWARE_GRANTS_TABLE,
                "Key": _ddb({"userSub": user_sub, "target": target}),
                "UpdateExpression": "SET #s=:revoked, revokedAt=:now, revokedBySub=:actor",
                "ConditionExpression": "attribute_exists(userSub)",
                "ExpressionAttributeNames": {"#s": "status"},
                "ExpressionAttributeValues": _ddb({
                    ":revoked": "REVOKED", ":now": now, ":actor": claims["sub"],
                }),
            }},
            {"Put": {"TableName": AUDIT_TABLE, "Item": _ddb(audit),
                     "ConditionExpression": "attribute_not_exists(entityId) AND attribute_not_exists(eventKey)"}},
        ])
    except ClientError:
        return _json(404, {"error": "grant_not_found"})
    return _json(200, {"status": "REVOKED", "target": target})


def firmware_access(claims: dict) -> dict:
    now = int(time.time())
    grant, release = _active_firmware_release(str(claims["sub"]), now)
    if not grant:
        return _json(200, {"authorized": False})
    return _json(200, {
        "authorized": True, "expiresAt": int(grant["expiresAt"]),
        "release": _public_release(release),
    })


def firmware_download(claims: dict) -> dict:
    now = int(time.time())
    grant, release = _active_firmware_release(str(claims["sub"]), now)
    if not grant:
        return _json(403, {"error": "firmware_not_authorized"})
    operation_id = secrets.token_urlsafe(18)
    audit = _audit(
        f"firmware#{claims['sub']}", "FIRMWARE_DOWNLOAD_AUTHORIZED", now,
        actorSub=claims["sub"], target=release["target"],
        version=release["version"], operationId=operation_id,
    )
    ddb_client.transact_write_items(TransactItems=[
        {"Put": {"TableName": AUDIT_TABLE, "Item": _ddb(audit),
                 "ConditionExpression": "attribute_not_exists(entityId) AND attribute_not_exists(eventKey)"}},
    ])
    url = s3_client.generate_presigned_url(
        "get_object",
        Params={"Bucket": FIRMWARE_BUCKET, "Key": release["artifactKey"]},
        ExpiresIn=FIRMWARE_URL_TTL_SECONDS,
    )
    return _json(200, {
        "operationId": operation_id, "url": url,
        "urlExpiresAt": now + FIRMWARE_URL_TTL_SECONDS,
        "release": _public_release(release),
    })


def firmware_result(event: dict, claims: dict) -> dict:
    body = _body(event)
    operation_id = str(body.get("operationId", ""))
    result = str(body.get("result", ""))
    target = str(body.get("target", ""))
    release = _firmware_release(target)
    if (not CLAIM_PART_RE.fullmatch(operation_id) or result not in {"SUCCEEDED", "FAILED"}
            or not release):
        return _json(400, {"error": "invalid_request"})
    now = int(time.time())
    audit = _audit(
        f"firmware#{claims['sub']}", f"FIRMWARE_FLASH_{result}", now,
        actorSub=claims["sub"], target=target,
        version=release["version"], operationId=operation_id,
        result=result,
    )
    ddb_client.transact_write_items(TransactItems=[
        {"Put": {"TableName": AUDIT_TABLE, "Item": _ddb(audit),
                 "ConditionExpression": "attribute_not_exists(entityId) AND attribute_not_exists(eventKey)"}},
    ])
    return _json(200, {"status": "recorded"})


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
    if user_sub and _claim_read_only(user_sub):
        return _json(403, {"error": "onboarding_read_only"})
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
    if route == "POST /api/firmware/grants":
        return firmware_grant(event, claims)
    if route == "POST /api/firmware/grants/revoke":
        return firmware_revoke(event, claims)
    if route == "GET /api/firmware/access":
        return firmware_access(claims)
    if route == "POST /api/firmware/download":
        return firmware_download(claims)
    if route == "POST /api/firmware/result":
        return firmware_result(event, claims)
    return _json(404, {"error": "not_found"})
