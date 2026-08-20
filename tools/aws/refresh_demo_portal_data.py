#!/usr/bin/env python3
"""Plan or refresh one isolated, frozen portal demo dataset.

The command is read-only unless --apply is supplied. It copies a bounded source
window to a demo-only vehicle identity, shifts the newest source timestamp to the
current time and injects one fixed location point. Cognito sends the invitation;
this tool never accepts or stores a password.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import re
import subprocess
import time
from typing import Any


EMAIL_RE = re.compile(r"^[^\s@]+@[^\s@]+\.[^\s@]+$")
VEHICLE_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,63}$")
DEMO_PREFIX = "demo-"
DAY_MS = 86_400_000
DAY_SECONDS = 86_400
HISTORY_SIGNALS = {"soc", "odometer", "charging", "plugged", "speed", "power"}
DEMO_RESOLUTIONS = ((DAY_MS, 300), (7 * DAY_MS, 1800), (30 * DAY_MS, 7200))
EXCLUDED_STATE_TOPICS = {
    "system/device_id",
    "system/device_name",
    "system/heartbeat",
    "system/ip_address",
    "system/mqtt_client_id",
    "system/last_seen_utc",
}


class DemoDataError(RuntimeError):
    pass


class AwsCli:
    def run(self, arguments: list[str]) -> dict[str, Any]:
        completed = subprocess.run(
            ["aws", *arguments, "--output", "json"],
            check=False, capture_output=True, text=True,
        )
        if completed.returncode:
            message = completed.stderr.strip().splitlines()[-1:] or ["AWS CLI failed"]
            raise DemoDataError(message[0])
        return json.loads(completed.stdout or "{}")


def stack_outputs(aws: AwsCli, stack_name: str, region: str) -> dict[str, str]:
    payload = aws.run([
        "cloudformation", "describe-stacks", "--stack-name", stack_name,
        "--region", region,
    ])
    stacks = payload.get("Stacks", [])
    if len(stacks) != 1:
        raise DemoDataError("target stack was not resolved uniquely")
    outputs = {
        item["OutputKey"]: item["OutputValue"]
        for item in stacks[0].get("Outputs", [])
    }
    required = {
        "CognitoUserPoolId", "UserVehicleAccessTableName",
        "VehicleStateTableName", "VehicleHistoryTableName",
    }
    missing = sorted(required - outputs.keys())
    if missing:
        raise DemoDataError(f"stack outputs missing: {', '.join(missing)}")
    return outputs


def query_partition(aws, table, key_name, key_value, region):
    items = []
    cursor = None
    while True:
        args = [
            "dynamodb", "query", "--table-name", table,
            "--key-condition-expression", f"{key_name} = :value",
            "--expression-attribute-values", json.dumps({":value": {"S": key_value}}),
            "--consistent-read", "--region", region,
        ]
        if cursor:
            args.extend(["--exclusive-start-key", json.dumps(cursor)])
        result = aws.run(args)
        items.extend(result.get("Items", []))
        cursor = result.get("LastEvaluatedKey")
        if not cursor:
            return items


def number(item, name, default=0):
    try:
        return int(item.get(name, {}).get("N", default))
    except (TypeError, ValueError):
        return default


def string(item, name, default=""):
    return str(item.get(name, {}).get("S", default))


def set_number(item, name, value):
    item[name] = {"N": str(int(value))}


def text_value(value):
    if "BOOL" in value:
        return "true" if value["BOOL"] else "false"
    if "N" in value:
        return value["N"]
    return value.get("S", "")


def state_location_item(vehicle_id, suffix, value, now_ms):
    value_text = str(value)
    return {
        "vehicleId": {"S": vehicle_id},
        "topicSuffix": {"S": suffix},
        "category": {"S": "location"},
        "fullTopic": {"S": f"mot/{vehicle_id}/{suffix}"},
        "value": {"N": value_text},
        "valueType": {"S": "number"},
        "payloadText": {"S": value_text},
        "payloadBytes": {"N": str(len(value_text.encode("utf-8")))},
        "receivedAt": {"N": str(now_ms)},
    }


def transform_state(items, source_id, target_id, shift_ms, now_ms, latitude, longitude):
    result = []
    for original in items:
        suffix = string(original, "topicSuffix")
        if suffix in EXCLUDED_STATE_TOPICS or suffix.startswith(("location/", "gps/")):
            continue
        item = json.loads(json.dumps(original))
        item["vehicleId"] = {"S": target_id}
        item["fullTopic"] = {"S": f"mot/{target_id}/{suffix}"}
        set_number(item, "receivedAt", min(now_ms, number(item, "receivedAt") + shift_ms))
        if suffix == "status/online":
            item["value"] = {"BOOL": False}
            item["valueType"] = {"S": "boolean"}
            item["payloadText"] = {"S": "false"}
            item["payloadBytes"] = {"N": "5"}
        result.append(item)
    result.extend([
        state_location_item(target_id, "location/latitude", latitude, now_ms),
        state_location_item(target_id, "location/longitude", longitude, now_ms),
    ])
    return result


def transform_history(items, target_id, source_latest_ms, now_ms, days):
    lower = source_latest_ms - days * DAY_MS
    shift_ms = now_ms - source_latest_ms
    buckets = {}
    numeric_aggregates = {}
    for original in items:
        sampled_at = number(original, "sampledAt")
        signal = string(original, "signal")
        if sampled_at < lower or sampled_at > source_latest_ms or signal not in HISTORY_SIGNALS:
            continue
        age_ms = source_latest_ms - sampled_at
        resolution = next(
            seconds for horizon, seconds in DEMO_RESOLUTIONS if age_ms <= horizon
        )
        bucket_seconds = (sampled_at // 1000 // resolution) * resolution
        key = (signal, bucket_seconds)
        if signal in {"speed", "power"} and "N" in original.get("value", {}):
            aggregate = numeric_aggregates.setdefault(key, {"sum": 0.0, "count": 0, "item": original})
            aggregate["sum"] += float(original["value"]["N"])
            aggregate["count"] += 1
            if sampled_at >= number(aggregate["item"], "sampledAt"):
                aggregate["item"] = original
        elif key not in buckets or sampled_at >= number(buckets[key], "sampledAt"):
            buckets[key] = original
    for key, aggregate in numeric_aggregates.items():
        item = json.loads(json.dumps(aggregate["item"]))
        average = aggregate["sum"] / aggregate["count"]
        item["value"] = {"N": f"{average:.1f}".rstrip("0").rstrip(".")}
        buckets[key] = item

    result = []
    for (signal, bucket_seconds), original in sorted(buckets.items()):
        item = json.loads(json.dumps(original))
        shifted_ms = bucket_seconds * 1000 + shift_ms
        shifted_seconds = shifted_ms // 1000
        item["vehicleId"] = {"S": target_id}
        item["sampleKey"] = {"S": f"{signal}#{shifted_seconds:010d}"}
        set_number(item, "sampledAt", shifted_ms)
        if "receivedAt" in item:
            set_number(item, "receivedAt", number(item, "receivedAt") + shift_ms)
        # Keep the complete shifted 30-day window available for another 31 days.
        set_number(item, "expiresAt", now_ms // 1000 + 31 * DAY_SECONDS)
        result.append(item)
    return result


def find_user(aws, pool_id, email, region):
    result = aws.run([
        "cognito-idp", "list-users", "--user-pool-id", pool_id,
        "--filter", f'email = "{email}"', "--region", region,
    ])
    users = result.get("Users", [])
    if len(users) > 1:
        raise DemoDataError("email resolved to multiple Cognito users")
    return users[0] if users else None


def user_attribute(user, name):
    for item in user.get("Attributes", []):
        if item.get("Name") == name:
            return str(item.get("Value", ""))
    return ""


def batch_requests(aws, table, requests, region):
    pending = list(requests)
    retries = 0
    while pending:
        chunk, pending = pending[:25], pending[25:]
        response = aws.run([
            "dynamodb", "batch-write-item", "--request-items",
            json.dumps({table: chunk}), "--region", region,
        ])
        unprocessed = response.get("UnprocessedItems", {}).get(table, [])
        if unprocessed:
            retries += 1
            if retries > 8:
                raise DemoDataError("DynamoDB batch writes remained unprocessed")
            pending = unprocessed + pending
            time.sleep(min(2 ** retries / 10, 2))
        else:
            retries = 0


def put_items(aws, table, items, region):
    batch_requests(aws, table, [{"PutRequest": {"Item": item}} for item in items], region)


def delete_items(aws, table, items, key_names, region):
    requests = [
        {"DeleteRequest": {"Key": {name: item[name] for name in key_names}}}
        for item in items
    ]
    batch_requests(aws, table, requests, region)


def refresh(args, aws, now_ms=None):
    email = args.email.strip().lower()
    source_id = args.source_vehicle.strip()
    target_id = args.target_vehicle.strip()
    if not EMAIL_RE.fullmatch(email):
        raise DemoDataError("invalid email address")
    if not VEHICLE_RE.fullmatch(source_id) or not VEHICLE_RE.fullmatch(target_id):
        raise DemoDataError("invalid vehicleId")
    if not target_id.startswith(DEMO_PREFIX):
        raise DemoDataError(f"target vehicleId must start with {DEMO_PREFIX}")
    if source_id == target_id:
        raise DemoDataError("source and target vehicleId must differ")
    if not 1 <= args.history_days <= 30:
        raise DemoDataError("history-days must be between 1 and 30")
    if not (-90 <= args.latitude <= 90 and -180 <= args.longitude <= 180):
        raise DemoDataError("invalid location")

    outputs = stack_outputs(aws, args.stack_name, args.region)
    state_table = outputs["VehicleStateTableName"]
    history_table = outputs["VehicleHistoryTableName"]
    access_table = outputs["UserVehicleAccessTableName"]
    source_state = query_partition(aws, state_table, "vehicleId", source_id, args.region)
    if not source_state:
        raise DemoDataError("source vehicle has no telemetry state")
    source_history = query_partition(aws, history_table, "vehicleId", source_id, args.region)
    if not source_history:
        raise DemoDataError("source vehicle has no history")
    source_latest_ms = max(number(item, "sampledAt") for item in source_history)
    now_ms = int(now_ms if now_ms is not None else time.time() * 1000)
    shift_ms = now_ms - max(number(item, "receivedAt") for item in source_state)
    new_state = transform_state(
        source_state, source_id, target_id, shift_ms, now_ms,
        args.latitude, args.longitude,
    )
    new_history = transform_history(
        source_history, target_id, source_latest_ms, now_ms, args.history_days,
    )
    old_state = query_partition(aws, state_table, "vehicleId", target_id, args.region)
    old_history = query_partition(aws, history_table, "vehicleId", target_id, args.region)
    user = find_user(aws, outputs["CognitoUserPoolId"], email, args.region)
    user_sub = user_attribute(user, "sub") if user else ""
    if user and not user_sub:
        raise DemoDataError("existing Cognito user has no sub")

    result = {
        "mode": "apply" if args.apply else "plan",
        "sourceVehicle": source_id,
        "targetVehicle": target_id,
        "stateItems": len(new_state),
        "historyItems": len(new_history),
        "replacesStateItems": len(old_state),
        "replacesHistoryItems": len(old_history),
        "historyDays": args.history_days,
        "user": "existing" if user else "invite-required",
        "location": {"latitude": args.latitude, "longitude": args.longitude},
    }
    if not args.apply:
        return result

    delete_items(aws, state_table, old_state, ("vehicleId", "topicSuffix"), args.region)
    delete_items(aws, history_table, old_history, ("vehicleId", "sampleKey"), args.region)
    put_items(aws, state_table, new_state, args.region)
    put_items(aws, history_table, new_history, args.region)

    if not user:
        created = aws.run([
            "cognito-idp", "admin-create-user",
            "--user-pool-id", outputs["CognitoUserPoolId"],
            "--username", email,
            "--user-attributes", f"Name=email,Value={email}",
            "--desired-delivery-mediums", "EMAIL", "--region", args.region,
        ])
        user = created.get("User", {})
        user_sub = user_attribute(user, "sub")
        if not user_sub:
            raise DemoDataError("created Cognito user has no sub")
        result["user"] = "invited"

    existing = aws.run([
        "dynamodb", "get-item", "--table-name", access_table,
        "--key", json.dumps({
            "userSub": {"S": user_sub}, "vehicleId": {"S": target_id},
        }), "--consistent-read", "--region", args.region,
    ]).get("Item")
    if existing and string(existing, "status") != "ACTIVE":
        raise DemoDataError("demo assignment exists but is not ACTIVE; review required")
    if not existing:
        now = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
        access_item = {
            "userSub": {"S": user_sub}, "vehicleId": {"S": target_id},
            "status": {"S": "ACTIVE"}, "role": {"S": "OWNER"},
            "createdAt": {"S": now}, "updatedAt": {"S": now},
            "source": {"S": "demo-001-static-copy"},
        }
        aws.run([
            "dynamodb", "put-item", "--table-name", access_table,
            "--item", json.dumps(access_item),
            "--condition-expression", "attribute_not_exists(userSub) AND attribute_not_exists(vehicleId)",
            "--region", args.region,
        ])
        result["assignment"] = "created-active-owner"
    else:
        result["assignment"] = "already-active"
    return result


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--stack-name", default="mot-aws-3-1")
    result.add_argument("--region", default="eu-north-1")
    result.add_argument("--email", default="demo@microlino-open-telemetry.ch")
    result.add_argument("--source-vehicle", default="xrpioneer2")
    result.add_argument("--target-vehicle", default="demo-pioneer")
    result.add_argument("--history-days", type=int, default=30)
    result.add_argument("--latitude", type=float, default=47.46268167287872)
    result.add_argument("--longitude", type=float, default=8.180829969601682)
    result.add_argument("--apply", action="store_true")
    return result


def main():
    args = parser().parse_args()
    try:
        result = refresh(args, AwsCli())
    except DemoDataError as error:
        message = str(error).replace(args.email, "[redacted-email]")
        print(json.dumps({"ok": False, "error": message}, separators=(",", ":")))
        return 2
    print(json.dumps({"ok": True, **result}, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
