#!/usr/bin/env python3
"""Validate the deployed comparison Lambda without emitting identities."""

import argparse
from collections import Counter
import json

import boto3


def scan_all(table):
    items = []
    scan = {}
    while True:
        page = table.scan(**scan)
        items.extend(page.get("Items", []))
        key = page.get("LastEvaluatedKey")
        if not key:
            return items
        scan["ExclusiveStartKey"] = key


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--month", required=True)
    parser.add_argument("--event-table", default="mot-dev-notification-events")
    parser.add_argument("--function-name", default="mot-dev-efficiency-comparison")
    args = parser.parse_args()

    ddb = boto3.resource("dynamodb", region_name=args.region)
    candidates = {}
    for item in scan_all(ddb.Table(args.event_table)):
        if item.get("eventType") != "JOURNEY_SUMMARY":
            continue
        received = int(item.get("receivedAt", 0))
        from datetime import datetime, timezone
        from zoneinfo import ZoneInfo
        month = datetime.fromtimestamp(received / 1000, tz=timezone.utc).astimezone(
            ZoneInfo("Europe/Zurich")
        ).strftime("%Y-%m")
        if month == args.month and item.get("userSub") and item.get("vehicleId"):
            candidates[(item["userSub"], item["vehicleId"])] = True

    client = boto3.client("lambda", region_name=args.region)
    flags = Counter()
    unavailable = Counter()
    response_months = Counter()
    for user_sub, vehicle_id in candidates:
        event = {
            "requestContext": {"authorizer": {"jwt": {"claims": {"sub": user_sub}}}},
            "pathParameters": {"vehicleId": vehicle_id},
        }
        response = client.invoke(
            FunctionName=args.function_name,
            InvocationType="RequestResponse",
            Payload=json.dumps(event).encode("utf-8"),
        )
        payload = json.loads(response["Payload"].read())
        if response.get("FunctionError") or payload.get("statusCode") != 200:
            raise RuntimeError("comparison Lambda invocation failed")
        body = json.loads(payload["body"])
        response_months[body.get("month", "unknown")] += 1
        serialized = json.dumps(body).lower()
        if "soc" in serialized or "battery" in serialized or user_sub in serialized or vehicle_id in serialized:
            raise RuntimeError("comparison response crossed the privacy contract")
        if body.get("available"):
            flags[body.get("flag")] += 1
        else:
            unavailable[body.get("reason", "unknown")] += 1
    print(json.dumps({
        "month": args.month,
        "comparisons": len(candidates),
        "available": sum(flags.values()),
        "flags": dict(sorted(flags.items())),
        "unavailable": dict(sorted(unavailable.items())),
        "responseMonths": dict(sorted(response_months.items())),
        "privacyContract": "passed",
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
