#!/usr/bin/env python3
"""Backfill anonymous monthly fleet efficiency from expiring journey events."""

import argparse
from collections import defaultdict
from decimal import Decimal
import json
from pathlib import Path
import sys
import time

import boto3
from botocore.exceptions import ClientError


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cloud/aws/notifications"))
from fleet_efficiency import (  # noqa: E402
    aggregate_values, record_active_vehicle, record_event, record_personal_event,
)


def scan_all(table, projection=None):
    kwargs = {}
    if projection:
        kwargs["ProjectionExpression"] = projection
    items = []
    while True:
        page = table.scan(**kwargs)
        items.extend(page.get("Items", []))
        key = page.get("LastEvaluatedKey")
        if not key:
            return items
        kwargs["ExclusiveStartKey"] = key


def unique_journeys(events):
    result = {}
    for item in events:
        if item.get("eventType") != "JOURNEY_SUMMARY":
            continue
        key = (str(item.get("vehicleId", "")), str(item.get("journeyId", "")))
        if not all(key):
            continue
        prior = result.get(key)
        if prior and any(
            Decimal(str(prior.get(field, 0))) != Decimal(str(item.get(field, 0)))
            for field in ("distanceKm", "energyNetKwh", "socUsed")
        ):
            raise RuntimeError("conflicting duplicate logical journey")
        result[key] = item
    return list(result.values())


def summary(journeys, profiles):
    months = defaultdict(lambda: defaultdict(Decimal))
    active = defaultdict(set)
    for event in journeys:
        values = aggregate_values(event, profiles.get(event["vehicleId"], {}))
        bucket = months[values["month"]]
        active[values["month"]].add(event["vehicleId"])
        for key, value in values.items():
            if key not in {"markerId", "month"}:
                bucket[key] += value
    for month, vehicles in active.items():
        months[month]["activeVehicleCount"] = Decimal(len(vehicles))
    return {
        month: {key: str(value) for key, value in sorted(values.items())}
        for month, values in sorted(months.items())
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--event-table", default="mot-dev-notification-events")
    parser.add_argument("--preference-table", default="mot-dev-notification-preferences")
    parser.add_argument("--profile-table", default="mot-dev-vehicle-profiles")
    parser.add_argument("--aggregate-table", default="mot-dev-fleet-efficiency-monthly")
    parser.add_argument("--marker-table", default="mot-dev-fleet-efficiency-markers")
    parser.add_argument("--personal-table", default="mot-dev-user-efficiency-monthly")
    parser.add_argument("--default-capacity-kwh", type=Decimal, default=Decimal("10.5"))
    parser.add_argument("--long-range-capacity-kwh", type=Decimal, default=Decimal("15"))
    parser.add_argument("--long-range-email-prefix", required=True)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--smoke-lambda-function")
    args = parser.parse_args()

    ddb = boto3.resource("dynamodb", region_name=args.region)
    client = boto3.client("dynamodb", region_name=args.region)
    all_events = [
        item for item in scan_all(ddb.Table(args.event_table))
        if item.get("eventType") == "JOURNEY_SUMMARY"
    ]
    events = unique_journeys(all_events)
    preferences = scan_all(ddb.Table(args.preference_table), "vehicleId,userSub,email")
    prefix = args.long_range_email_prefix.strip().lower()
    long_vehicles = {
        item["vehicleId"] for item in preferences
        if str(item.get("email", "")).lower().startswith(prefix)
    }
    if len(long_vehicles) != 1:
        raise RuntimeError("long-range email selector must resolve exactly one vehicle")
    event_vehicles = {item["vehicleId"] for item in events}
    profiles = {
        vehicle_id: {
            "batteryCapacityKwh": (
                args.long_range_capacity_kwh
                if vehicle_id in long_vehicles else args.default_capacity_kwh
            ),
            "batteryCapacityVerificationStatus": "DECLARED",
        }
        for vehicle_id in event_vehicles
    }
    result = {
        "mode": "apply" if args.apply else "dry-run",
        "logicalJourneys": len(events),
        "personalJourneyEvents": len(all_events),
        "vehicles": len(event_vehicles),
        "longRangeVehicles": len(long_vehicles),
        "months": summary(events, profiles),
    }
    if args.smoke_lambda_function:
        from boto3.dynamodb.types import TypeSerializer

        serializer = TypeSerializer()
        sample = min(events, key=lambda item: int(item["receivedAt"]))
        payload = {
            "Records": [{
                "eventName": "INSERT",
                "dynamodb": {
                    "NewImage": {
                        key: serializer.serialize(value)
                        for key, value in sample.items()
                    }
                },
            }]
        }
        response = boto3.client("lambda", region_name=args.region).invoke(
            FunctionName=args.smoke_lambda_function,
            InvocationType="RequestResponse",
            Payload=json.dumps(payload).encode("utf-8"),
        )
        body = json.loads(response["Payload"].read())
        if response.get("FunctionError") or body != {
            "recorded": 0, "duplicates": 1, "ignored": 0,
            "activeAdded": 0, "personalRecorded": 0,
        }:
            raise RuntimeError("deployed Lambda duplicate smoke test failed")
        result["lambdaSmoke"] = body
    if not args.apply:
        print(json.dumps(result, indent=2, sort_keys=True))
        return

    now = int(time.time())
    profile_table = ddb.Table(args.profile_table)
    for vehicle_id, profile in profiles.items():
        item = {
            "vehicleId": vehicle_id,
            **profile,
            "batteryCapacitySource": "MAINTAINER_DECLARED_BACKFILL",
            "batteryCapacityDeclaredAt": now * 1000,
            "schemaVersion": 1,
        }
        try:
            profile_table.put_item(
                Item=item, ConditionExpression="attribute_not_exists(vehicleId)"
            )
        except ClientError as error:
            if error.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
                raise
            existing = profile_table.get_item(
                Key={"vehicleId": vehicle_id}, ConsistentRead=True
            ).get("Item", {})
            if (
                Decimal(str(existing.get("batteryCapacityKwh", 0)))
                != profile["batteryCapacityKwh"]
                or existing.get("batteryCapacityVerificationStatus") not in {"DECLARED", "PLAUSIBLE"}
            ):
                raise RuntimeError("existing vehicle profile conflicts with declared backfill capacity")

    recorded = 0
    duplicate = 0
    active_added = 0
    personal_recorded = 0
    for event in sorted(events, key=lambda item: int(item["receivedAt"])):
        if record_event(
            client,
            args.marker_table,
            args.aggregate_table,
            profile_table,
            event,
            now=now,
        ):
            recorded += 1
        else:
            duplicate += 1
        if record_active_vehicle(
            client, args.marker_table, args.aggregate_table, event, now=now
        ):
            active_added += 1
    for event in sorted(all_events, key=lambda item: int(item["receivedAt"])):
        if record_personal_event(
            client, args.marker_table, args.personal_table, event, now=now
        ):
            personal_recorded += 1
    result.update({
        "recorded": recorded,
        "duplicates": duplicate,
        "activeVehiclesAdded": active_added,
        "personalRecorded": personal_recorded,
    })
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
