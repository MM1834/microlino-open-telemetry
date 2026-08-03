#!/usr/bin/env python3
"""Plan or apply one controlled beta invitation and vehicle assignment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import re
import subprocess
from typing import Any


EMAIL_RE = re.compile(r"^[^\s@]+@[^\s@]+\.[^\s@]+$")
VEHICLE_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,63}$")
SOURCE_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")


class OnboardingError(RuntimeError):
    pass


class AwsCli:
    def run(self, arguments: list[str]) -> dict[str, Any]:
        command = ["aws", *arguments, "--output", "json"]
        completed = subprocess.run(
            command, check=False, capture_output=True, text=True
        )
        if completed.returncode:
            message = completed.stderr.strip().splitlines()[-1:] or ["AWS CLI failed"]
            raise OnboardingError(message[0])
        return json.loads(completed.stdout or "{}")


def stack_outputs(aws: AwsCli, stack_name: str, region: str) -> dict[str, str]:
    payload = aws.run([
        "cloudformation", "describe-stacks", "--stack-name", stack_name,
        "--region", region,
    ])
    stacks = payload.get("Stacks", [])
    if len(stacks) != 1:
        raise OnboardingError("target stack was not resolved uniquely")
    outputs = {
        item["OutputKey"]: item["OutputValue"]
        for item in stacks[0].get("Outputs", [])
    }
    required = {
        "CognitoUserPoolId", "UserVehicleAccessTableName", "VehicleStateTableName"
    }
    missing = sorted(required - outputs.keys())
    if missing:
        raise OnboardingError(f"stack outputs missing: {', '.join(missing)}")
    return outputs


def find_user(aws: AwsCli, pool_id: str, email: str, region: str) -> dict | None:
    payload = aws.run([
        "cognito-idp", "list-users", "--user-pool-id", pool_id,
        "--filter", f'email = "{email}"', "--region", region,
    ])
    users = payload.get("Users", [])
    if len(users) > 1:
        raise OnboardingError("email resolved to multiple Cognito users")
    return users[0] if users else None


def attribute(user: dict, name: str) -> str:
    for item in user.get("Attributes", []):
        if item.get("Name") == name:
            return str(item.get("Value", ""))
    return ""


def vehicle_exists(aws: AwsCli, table: str, vehicle_id: str, region: str) -> bool:
    payload = aws.run([
        "dynamodb", "query", "--table-name", table,
        "--key-condition-expression", "vehicleId = :vehicle",
        "--expression-attribute-values",
        json.dumps({":vehicle": {"S": vehicle_id}}),
        "--limit", "1", "--consistent-read", "--region", region,
    ])
    return bool(payload.get("Items"))


def active_owner_subjects(
    aws: AwsCli, table: str, vehicle_id: str, region: str
) -> set[str]:
    payload = aws.run([
        "dynamodb", "scan", "--table-name", table,
        "--filter-expression", "vehicleId = :vehicle AND #status = :active",
        "--expression-attribute-names", json.dumps({"#status": "status"}),
        "--expression-attribute-values",
        json.dumps({":vehicle": {"S": vehicle_id}, ":active": {"S": "ACTIVE"}}),
        "--projection-expression", "userSub", "--consistent-read", "--region", region,
    ])
    if payload.get("LastEvaluatedKey"):
        raise OnboardingError("assignment scan incomplete; review required")
    return {
        item.get("userSub", {}).get("S", "")
        for item in payload.get("Items", [])
        if item.get("userSub", {}).get("S")
    }


def assignment(aws: AwsCli, table: str, user_sub: str, vehicle_id: str, region: str):
    payload = aws.run([
        "dynamodb", "get-item", "--table-name", table,
        "--key", json.dumps({
            "userSub": {"S": user_sub}, "vehicleId": {"S": vehicle_id}
        }),
        "--consistent-read", "--region", region,
    ])
    return payload.get("Item")


def create_user(aws: AwsCli, pool_id: str, email: str, region: str) -> dict:
    payload = aws.run([
        "cognito-idp", "admin-create-user", "--user-pool-id", pool_id,
        "--username", email, "--user-attributes", f"Name=email,Value={email}",
        "--desired-delivery-mediums", "EMAIL", "--region", region,
    ])
    return payload["User"]


def put_assignment(
    aws: AwsCli, table: str, user_sub: str, vehicle_id: str, source: str,
    region: str, now: str,
) -> None:
    item = {
        "userSub": {"S": user_sub}, "vehicleId": {"S": vehicle_id},
        "status": {"S": "ACTIVE"}, "role": {"S": "OWNER"},
        "createdAt": {"S": now}, "updatedAt": {"S": now},
        "source": {"S": source},
    }
    aws.run([
        "dynamodb", "put-item", "--table-name", table,
        "--item", json.dumps(item),
        "--condition-expression",
        "attribute_not_exists(userSub) AND attribute_not_exists(vehicleId)",
        "--region", region,
    ])


def onboard(args, aws: AwsCli) -> dict[str, Any]:
    email = args.email.strip().lower()
    vehicle_id = args.vehicle_id.strip()
    if not EMAIL_RE.fullmatch(email):
        raise OnboardingError("invalid email address")
    if not VEHICLE_RE.fullmatch(vehicle_id):
        raise OnboardingError("invalid vehicleId")
    if not SOURCE_RE.fullmatch(args.source):
        raise OnboardingError("invalid non-personal source reference")

    outputs = stack_outputs(aws, args.stack_name, args.region)
    if not vehicle_exists(
        aws, outputs["VehicleStateTableName"], vehicle_id, args.region
    ):
        raise OnboardingError("vehicleId has no telemetry state")

    user = find_user(aws, outputs["CognitoUserPoolId"], email, args.region)
    user_sub = attribute(user, "sub") if user else ""
    if user and not user_sub:
        raise OnboardingError("existing Cognito user has no sub")
    owners = active_owner_subjects(
        aws, outputs["UserVehicleAccessTableName"], vehicle_id, args.region
    )
    if owners and (not user_sub or owners != {user_sub}):
        raise OnboardingError("vehicleId already has another ACTIVE owner")

    existing = assignment(
        aws, outputs["UserVehicleAccessTableName"], user_sub, vehicle_id, args.region
    ) if user_sub else None
    if existing:
        status = existing.get("status", {}).get("S")
        if status == "ACTIVE":
            return {
                "mode": "apply" if args.apply else "plan",
                "user": "existing", "assignment": "already-active",
                "vehicleId": vehicle_id,
            }
        raise OnboardingError("assignment exists but is not ACTIVE; review required")

    result = {
        "mode": "apply" if args.apply else "plan",
        "user": "existing" if user else "invite-required",
        "assignment": "create-required", "vehicleId": vehicle_id,
    }
    if not args.apply:
        return result

    if not user:
        user = create_user(aws, outputs["CognitoUserPoolId"], email, args.region)
        user_sub = attribute(user, "sub")
        if not user_sub:
            raise OnboardingError("created user has no Cognito sub")
        result["user"] = "invited"

    now = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace(
        "+00:00", "Z"
    )
    put_assignment(
        aws, outputs["UserVehicleAccessTableName"], user_sub, vehicle_id,
        args.source, args.region, now,
    )
    result["assignment"] = "created-active-owner"
    return result


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--stack-name", default="mot-aws-3-1")
    result.add_argument("--region", default="eu-north-1")
    result.add_argument("--email", required=True)
    result.add_argument("--vehicle-id", required=True)
    result.add_argument("--source", default="onb-001-b1-admin")
    result.add_argument(
        "--apply", action="store_true",
        help="Apply invitation/assignment; without this flag only plan.",
    )
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        result = onboard(args, AwsCli())
    except OnboardingError as error:
        message = str(error).replace(args.email, "[redacted-email]")
        print(json.dumps({"ok": False, "error": message}, separators=(",", ":")))
        return 2
    print(json.dumps({"ok": True, **result}, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
