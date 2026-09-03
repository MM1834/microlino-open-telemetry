#!/usr/bin/env python3
"""Export one bounded MOT vehicle-debug capture to CSV."""

import argparse
import csv
import json
import re
import subprocess
from datetime import datetime, timezone
from decimal import Decimal
from pathlib import Path


VEHICLE_ID = re.compile(r"^[a-z0-9][a-z0-9_-]{0,63}$")


def epoch_ms(value: str) -> int:
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None:
        raise argparse.ArgumentTypeError("timestamps must include a UTC offset or Z")
    return int(parsed.timestamp() * 1000)


def scalar(attribute):
    if "N" in attribute:
        value = Decimal(attribute["N"])
        return int(value) if value == value.to_integral_value() else float(value)
    if "BOOL" in attribute:
        return attribute["BOOL"]
    if "S" in attribute:
        return attribute["S"]
    return ""


def iso_utc(value: int) -> str:
    return datetime.fromtimestamp(value / 1000, timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def query_items(table_name: str, region: str, vehicle_id: str, start_ms: int, end_ms: int):
    lower = f"{start_ms:013d}#"
    upper = f"{end_ms:013d}#\uffff"
    values = {
        ":vehicle": {"S": vehicle_id},
        ":start": {"S": lower},
        ":end": {"S": upper},
    }
    items = []
    last_key = None
    while True:
        command = [
            "aws", "dynamodb", "query", "--region", region,
            "--table-name", table_name,
            "--key-condition-expression",
            "vehicleId = :vehicle AND sampleKey BETWEEN :start AND :end",
            "--expression-attribute-values", json.dumps(values, separators=(",", ":")),
            "--consistent-read", "--output", "json",
        ]
        if last_key:
            command.extend(["--exclusive-start-key", json.dumps(last_key, separators=(",", ":"))])
        result = json.loads(subprocess.run(
            command, check=True, text=True, capture_output=True
        ).stdout)
        items.extend(result.get("Items", []))
        last_key = result.get("LastEvaluatedKey")
        if not last_key:
            return items


def write_csv(path: Path, items):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "vehicle_id", "sample_key", "received_at_utc", "topic_suffix",
            "value", "value_type", "expires_at_utc",
        ])
        for item in items:
            writer.writerow([
                item["vehicleId"]["S"], item["sampleKey"]["S"],
                iso_utc(int(item["receivedAt"]["N"])), item["topicSuffix"]["S"],
                scalar(item.get("value", {})), item.get("valueType", {}).get("S", ""),
                datetime.fromtimestamp(int(item["expiresAt"]["N"]), timezone.utc).isoformat().replace("+00:00", "Z"),
            ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vehicle-id", required=True)
    parser.add_argument("--start", required=True, type=epoch_ms, help="ISO-8601 timestamp with offset")
    parser.add_argument("--end", required=True, type=epoch_ms, help="ISO-8601 timestamp with offset")
    parser.add_argument("--table-name", default="mot-dev-vehicle-debug-history")
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if not VEHICLE_ID.fullmatch(args.vehicle_id):
        parser.error("invalid vehicle ID")
    if args.end < args.start:
        parser.error("end must not precede start")

    items = query_items(
        args.table_name, args.region, args.vehicle_id, args.start, args.end
    )
    write_csv(args.output, items)
    print(f"Exported {len(items)} rows to {args.output}")


if __name__ == "__main__":
    main()
