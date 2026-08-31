#!/usr/bin/env python3
"""Read-only, privacy-safe preflight for one user/vehicle SMS association."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import re
import subprocess
import time
from typing import Any


PHONE_RE = re.compile(r"^\+(41|49)[1-9][0-9]{7,11}$")


def aws(*args: str) -> dict[str, Any]:
    result = subprocess.run(
        ["aws", *args, "--output", "json"],
        check=False, capture_output=True, text=True,
    )
    if result.returncode:
        raise RuntimeError(result.stderr.strip().splitlines()[-1])
    return json.loads(result.stdout or "{}")


def value(item: dict[str, Any], name: str, kind: str, default: Any = None) -> Any:
    return item.get(name, {}).get(kind, default)


def get_item(region: str, table: str, key: dict[str, Any]) -> dict[str, Any]:
    return aws(
        "dynamodb", "get-item", "--region", region, "--table-name", table,
        "--key", json.dumps(key), "--consistent-read",
    ).get("Item", {})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    identity = parser.add_mutually_exclusive_group(required=True)
    identity.add_argument("--username")
    identity.add_argument("--user-sub")
    parser.add_argument("--vehicle-id", required=True)
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--stack-name", default="mot-dev-notifications")
    parser.add_argument("--user-pool-id", default="eu-north-1_vbMnyGtc0")
    args = parser.parse_args()

    lookup = args.username or args.user_sub
    user = aws(
        "cognito-idp", "admin-get-user", "--region", args.region,
        "--user-pool-id", args.user_pool_id, "--username", lookup,
    )
    attributes = {entry["Name"]: entry["Value"] for entry in user["UserAttributes"]}
    user_sub = attributes["sub"]
    if args.user_sub and user_sub != args.user_sub:
        raise RuntimeError("Cognito subject does not match resolved user")

    stack = aws(
        "cloudformation", "describe-stacks", "--region", args.region,
        "--stack-name", args.stack_name,
    )["Stacks"][0]
    parameters = {entry["ParameterKey"]: entry["ParameterValue"] for entry in stack["Parameters"]}
    outputs = {entry["OutputKey"]: entry["OutputValue"] for entry in stack["Outputs"]}

    key = {"vehicleId": {"S": args.vehicle_id}, "userSub": {"S": user_sub}}
    access_key = {"userSub": {"S": user_sub}, "vehicleId": {"S": args.vehicle_id}}
    preference = get_item(args.region, outputs["PreferenceTableName"], key)
    access = get_item(args.region, parameters["AccessTableName"], access_key)
    approval = get_item(args.region, outputs["SmsApprovalTableName"], key)

    phone = str(value(preference, "phoneE164", "S", ""))
    fingerprint = hashlib.sha256(phone.encode()).hexdigest() if phone else ""
    destination = get_item(
        args.region, outputs["SmsDestinationTableName"],
        {"destinationFingerprint": {"S": fingerprint}},
    ) if fingerprint else {}

    function = aws(
        "lambda", "get-function-configuration", "--region", args.region,
        "--function-name", "mot-dev-notifications",
    )
    environment = function.get("Environment", {}).get("Variables", {})
    limits = aws("pinpoint-sms-voice-v2", "describe-spend-limits", "--region", args.region)
    text_limits = [
        entry for entry in limits.get("SpendLimits", [])
        if entry.get("Name") == "TEXT_MESSAGE_MONTHLY_SPEND_LIMIT"
    ]
    alarm_name = environment.get("SMS_SPEND_ALARM_NAME", "")
    alarms = aws(
        "cloudwatch", "describe-alarms", "--region", args.region,
        "--alarm-names", alarm_name,
    ).get("MetricAlarms", [])

    now = int(time.time())
    country = "CH" if phone.startswith("+41") else "DE" if phone.startswith("+49") else ""
    destination_id = str(value(destination, "verifiedDestinationNumberId", "S", ""))
    authorized_destinations = {
        parameters.get("SmsVerifiedDestinationNumberArn", ""),
        parameters.get("SmsAdditionalVerifiedDestinationNumberArn", ""),
    }
    enforced = text_limits[0].get("EnforcedLimit") if len(text_limits) == 1 else None
    expected = environment.get("SMS_EXPECTED_SPEND_LIMIT_USD")
    expiry = int(value(approval, "expiresAt", "N", "0"))

    gates = {
        "user_status_confirmed": user.get("UserStatus") == "CONFIRMED",
        "vehicle_access_active": value(access, "status", "S") == "ACTIVE",
        "sms_opt_in_enabled": value(preference, "smsEnabled", "BOOL") is True,
        "sms_destination_confirmed": value(preference, "smsConfirmed", "BOOL") is True,
        "destination_format_allowed": bool(PHONE_RE.fullmatch(phone)),
        "destination_verified": value(destination, "status", "S") == "VERIFIED",
        "destination_iam_authorized": any(
            destination_id and arn.endswith("/" + destination_id)
            for arn in authorized_destinations
        ),
        "approval_active": value(approval, "status", "S") == "ACTIVE" and expiry > now,
        "approval_matches_destination": value(approval, "destinationFingerprint", "S") == fingerprint,
        "approval_matches_country": value(approval, "isoCountryCode", "S") == country,
        "originator_is_mot": value(approval, "originator", "S") == "MOT",
        "global_sms_switch_enabled": environment.get("SMS_DELIVERY_ENABLED") == "true",
        "spend_alarm_ok": len(alarms) == 1 and alarms[0].get("StateValue") == "OK",
        "spend_limit_matches_lambda": enforced is not None and str(enforced).rstrip(".0") == str(expected).rstrip(".0"),
    }
    result = {
        "identity": args.username or "cognito-sub",
        "vehicleId": args.vehicle_id,
        "ready": all(gates.values()),
        "approvalExpiresAt": datetime.fromtimestamp(expiry, timezone.utc).isoformat() if expiry else None,
        "spendAlarmThresholdUsd": alarms[0].get("Threshold") if len(alarms) == 1 else None,
        "enforcedSpendLimitUsd": enforced,
        "gates": gates,
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["ready"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
