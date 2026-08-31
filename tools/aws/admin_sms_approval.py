#!/usr/bin/env python3
"""Plan or apply one bounded SMS administrator approval or revocation."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import getpass
import hashlib
import json
import os
import re
import subprocess
import time
from typing import Any
import uuid


PHONE_RE = re.compile(r"^\+(41|49)[1-9][0-9]{7,11}$")
VEHICLE_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,63}$")
SUB_RE = re.compile(r"^[A-Za-z0-9:_-]{1,128}$")
REASON_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")
APPROVED_ORIGINATOR = "MOT"


class ApprovalError(RuntimeError):
    pass


class AwsCli:
    def __init__(self, env: dict[str, str] | None = None):
        self.env = env

    def run(self, arguments: list[str]) -> dict[str, Any]:
        command = ["aws", *arguments, "--output", "json"]
        completed = subprocess.run(
            command, check=False, capture_output=True, text=True, env=self.env
        )
        if completed.returncode:
            message = completed.stderr.strip().splitlines()[-1:] or ["AWS CLI failed"]
            raise ApprovalError(message[0])
        return json.loads(completed.stdout or "{}")

    def assume(self, role_arn: str, region: str) -> "AwsCli":
        payload = self.run([
            "sts", "assume-role", "--role-arn", role_arn,
            "--role-session-name", "sms-approval-admin", "--duration-seconds", "900",
            "--region", region,
        ])
        credentials = payload.get("Credentials", {})
        required = ("AccessKeyId", "SecretAccessKey", "SessionToken")
        if any(not credentials.get(key) for key in required):
            raise ApprovalError("assume-role returned incomplete credentials")
        env = dict(os.environ)
        env.update({
            "AWS_ACCESS_KEY_ID": credentials["AccessKeyId"],
            "AWS_SECRET_ACCESS_KEY": credentials["SecretAccessKey"],
            "AWS_SESSION_TOKEN": credentials["SessionToken"],
        })
        return AwsCli(env)


def stack_outputs(aws: AwsCli, stack_name: str, region: str) -> dict[str, str]:
    payload = aws.run([
        "cloudformation", "describe-stacks", "--stack-name", stack_name,
        "--region", region,
    ])
    stacks = payload.get("Stacks", [])
    if len(stacks) != 1:
        raise ApprovalError("target stack was not resolved uniquely")
    outputs = {
        item["OutputKey"]: item["OutputValue"]
        for item in stacks[0].get("Outputs", [])
    }
    required = {
        "SmsApprovalTableName", "SmsApprovalAuditTableName",
        "SmsApprovalAuditRetentionDays", "SmsApprovalAdminRoleArn",
    }
    missing = sorted(required - outputs.keys())
    if missing:
        raise ApprovalError(f"stack outputs missing: {', '.join(missing)}")
    return outputs


def identity_arn(aws: AwsCli, region: str) -> str:
    payload = aws.run(["sts", "get-caller-identity", "--region", region])
    arn = str(payload.get("Arn", ""))
    if not arn.startswith("arn:"):
        raise ApprovalError("caller identity is unavailable")
    return arn


def normalized_phone(value: str) -> str:
    phone = re.sub(r"[\s()-]", "", value.strip())
    if not PHONE_RE.fullmatch(phone):
        raise ApprovalError("destination must be a +41 or +49 E.164 number")
    return phone


def destination_fingerprint(phone: str) -> str:
    return hashlib.sha256(phone.encode()).hexdigest()


def get_item(
    aws: AwsCli, table: str, key: dict[str, dict[str, str]], region: str
) -> dict[str, Any]:
    return aws.run([
        "dynamodb", "get-item", "--table-name", table,
        "--key", json.dumps(key), "--consistent-read", "--region", region,
    ]).get("Item", {})


def active_access(aws: AwsCli, table: str, user_sub: str, vehicle_id: str, region: str):
    item = get_item(aws, table, {
        "userSub": {"S": user_sub}, "vehicleId": {"S": vehicle_id},
    }, region)
    return item.get("status", {}).get("S") == "ACTIVE"


def existing_approval(
    aws: AwsCli, table: str, user_sub: str, vehicle_id: str, region: str
) -> dict[str, Any]:
    return get_item(aws, table, {
        "vehicleId": {"S": vehicle_id}, "userSub": {"S": user_sub},
    }, region)


def n(item: dict[str, Any], key: str, default: int = 0) -> int:
    return int(item.get(key, {}).get("N", default))


def s(item: dict[str, Any], key: str) -> str:
    return str(item.get(key, {}).get("S", ""))


def approval_item(
    *, user_sub: str, vehicle_id: str, fingerprint: str, operator: str,
    reason: str, now: int, expires_at: int, version: int, country: str,
) -> dict[str, dict[str, str]]:
    return {
        "vehicleId": {"S": vehicle_id},
        "userSub": {"S": user_sub},
        "channel": {"S": "SMS"},
        "status": {"S": "ACTIVE"},
        "destinationFingerprint": {"S": fingerprint},
        "isoCountryCode": {"S": country},
        "originator": {"S": APPROVED_ORIGINATOR},
        "approvedBy": {"S": operator},
        "approvalReason": {"S": reason},
        "createdAt": {"N": str(now)},
        "updatedAt": {"N": str(now)},
        "expiresAt": {"N": str(expires_at)},
        "version": {"N": str(version)},
    }


def audit_item(
    *, event_id: str, action: str, user_sub: str, vehicle_id: str,
    fingerprint: str, operator: str, reason: str, now: int, expires_at: int,
    approval_version: int, country: str,
) -> dict[str, dict[str, str]]:
    return {
        "eventId": {"S": event_id},
        "action": {"S": action},
        "channel": {"S": "SMS"},
        "vehicleId": {"S": vehicle_id},
        "userSub": {"S": user_sub},
        "destinationFingerprint": {"S": fingerprint},
        "isoCountryCode": {"S": country},
        "originator": {"S": APPROVED_ORIGINATOR},
        "operator": {"S": operator},
        "reason": {"S": reason},
        "approvalVersion": {"N": str(approval_version)},
        "createdAt": {"N": str(now)},
        "expiresAt": {"N": str(expires_at)},
    }


def transact_approve(
    aws: AwsCli, outputs: dict[str, str], existing: dict[str, Any],
    approval: dict[str, Any], audit: dict[str, Any], region: str,
) -> None:
    put = {
        "TableName": outputs["SmsApprovalTableName"], "Item": approval,
    }
    if existing:
        put.update({
            "ConditionExpression": "version = :expected",
            "ExpressionAttributeValues": {":expected": existing["version"]},
        })
    else:
        put["ConditionExpression"] = (
            "attribute_not_exists(vehicleId) AND attribute_not_exists(userSub)"
        )
    transaction = [
        {"Put": put},
        {"Put": {
            "TableName": outputs["SmsApprovalAuditTableName"], "Item": audit,
            "ConditionExpression": "attribute_not_exists(eventId)",
        }},
    ]
    aws.run([
        "dynamodb", "transact-write-items", "--transact-items",
        json.dumps(transaction), "--region", region,
    ])


def transact_revoke(
    aws: AwsCli, outputs: dict[str, str], existing: dict[str, Any],
    audit: dict[str, Any], now: int, region: str,
) -> None:
    version = n(existing, "version")
    transaction = [
        {"Update": {
            "TableName": outputs["SmsApprovalTableName"],
            "Key": {
                "vehicleId": existing["vehicleId"], "userSub": existing["userSub"],
            },
            "UpdateExpression": (
                "SET #status=:revoked, revokedAt=:now, updatedAt=:now, version=:next"
            ),
            "ConditionExpression": "#status=:active AND version=:expected",
            "ExpressionAttributeNames": {"#status": "status"},
            "ExpressionAttributeValues": {
                ":revoked": {"S": "REVOKED"}, ":active": {"S": "ACTIVE"},
                ":now": {"N": str(now)}, ":next": {"N": str(version + 1)},
                ":expected": {"N": str(version)},
            },
        }},
        {"Put": {
            "TableName": outputs["SmsApprovalAuditTableName"], "Item": audit,
            "ConditionExpression": "attribute_not_exists(eventId)",
        }},
    ]
    aws.run([
        "dynamodb", "transact-write-items", "--transact-items",
        json.dumps(transaction), "--region", region,
    ])


def operate(args, bootstrap: AwsCli) -> dict[str, Any]:
    user_sub = args.user_sub.strip()
    vehicle_id = args.vehicle_id.strip()
    reason = args.reason.strip()
    if not SUB_RE.fullmatch(user_sub):
        raise ApprovalError("invalid Cognito subject")
    if not VEHICLE_RE.fullmatch(vehicle_id):
        raise ApprovalError("invalid vehicleId")
    if not REASON_RE.fullmatch(reason):
        raise ApprovalError("reason must be a non-personal reference token")
    if not 1 <= args.expires_days <= 90:
        raise ApprovalError("expires-days must be between 1 and 90")
    supplied_phone = args.phone_e164
    if supplied_phone is None:
        supplied_phone = getpass.getpass("Swiss destination in E.164 form: ")
    phone = normalized_phone(supplied_phone)
    fingerprint = destination_fingerprint(phone)
    approved_country = "CH" if phone.startswith("+41") else "DE"

    outputs = stack_outputs(bootstrap, args.stack_name, args.region)
    operator = identity_arn(bootstrap, args.region)
    aws = bootstrap.assume(outputs["SmsApprovalAdminRoleArn"], args.region)

    access_table = args.access_table_name
    if not active_access(aws, access_table, user_sub, vehicle_id, args.region):
        raise ApprovalError("user does not have ACTIVE vehicle access")
    existing = existing_approval(
        aws, outputs["SmsApprovalTableName"], user_sub, vehicle_id, args.region
    )
    now = int(time.time())
    current_version = n(existing, "version")
    result = {
        "mode": "apply" if args.apply else "plan",
        "action": args.action,
        "vehicleId": vehicle_id,
        "country": approved_country,
        "originator": APPROVED_ORIGINATOR,
        "destinationFingerprint": fingerprint,
        "currentStatus": s(existing, "status") or "NONE",
    }

    if args.action == "revoke":
        if not existing or s(existing, "status") != "ACTIVE":
            raise ApprovalError("no ACTIVE approval to revoke")
        next_version = current_version + 1
        if not args.apply:
            return {**result, "nextStatus": "REVOKED", "nextVersion": next_version}
        audit_expiry = now + int(outputs["SmsApprovalAuditRetentionDays"]) * 86400
        audit = audit_item(
            event_id=str(uuid.uuid4()), action="REVOKE", user_sub=user_sub,
            vehicle_id=vehicle_id, fingerprint=s(existing, "destinationFingerprint"),
            operator=operator, reason=reason, now=now, expires_at=audit_expiry,
            approval_version=next_version,
            country=s(existing, "isoCountryCode"),
        )
        transact_revoke(aws, outputs, existing, audit, now, args.region)
        return {**result, "nextStatus": "REVOKED", "nextVersion": next_version}

    expires_at = now + args.expires_days * 86400
    next_version = current_version + 1
    if not args.apply:
        return {
            **result, "nextStatus": "ACTIVE", "nextVersion": next_version,
            "expiresAt": expires_at,
        }
    approval = approval_item(
        user_sub=user_sub, vehicle_id=vehicle_id, fingerprint=fingerprint,
        operator=operator, reason=reason, now=now, expires_at=expires_at,
        version=next_version,
        country=approved_country,
    )
    audit_expiry = now + int(outputs["SmsApprovalAuditRetentionDays"]) * 86400
    audit = audit_item(
        event_id=str(uuid.uuid4()), action="APPROVE", user_sub=user_sub,
        vehicle_id=vehicle_id, fingerprint=fingerprint, operator=operator,
        reason=reason, now=now, expires_at=audit_expiry,
        approval_version=next_version,
        country=approved_country,
    )
    transact_approve(aws, outputs, existing, approval, audit, args.region)
    return {
        **result, "nextStatus": "ACTIVE", "nextVersion": next_version,
        "expiresAt": expires_at,
    }


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("action", choices=("approve", "revoke"))
    result.add_argument("--stack-name", default="mot-dev-notifications")
    result.add_argument("--region", default="eu-north-1")
    result.add_argument("--access-table-name", default="mot-dev-user-vehicle-access")
    result.add_argument("--user-sub", required=True)
    result.add_argument("--vehicle-id", required=True)
    result.add_argument(
        "--phone-e164",
        help="Swiss E.164 destination; omit to enter it without shell history.",
    )
    result.add_argument("--expires-days", type=int, default=30)
    result.add_argument("--reason", default="sms-001-controlled-pilot")
    result.add_argument(
        "--apply", action="store_true",
        help="Apply the approval mutation; without this flag only plan.",
    )
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        result = operate(args, AwsCli())
    except ApprovalError as error:
        message = str(error)
        if args.phone_e164:
            message = message.replace(args.phone_e164, "[redacted-phone]")
        message = message.replace(args.user_sub, "[redacted-sub]")
        print(json.dumps({"ok": False, "error": message}, separators=(",", ":")))
        return 2
    print(json.dumps({"ok": True, **result}, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
