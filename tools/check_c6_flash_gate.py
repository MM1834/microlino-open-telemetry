#!/usr/bin/env python3
"""Fail when a C6 application binary exceeds its configured OTA-slot budget."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
C6 = ROOT / "firmware" / "esp32-c6"


def app_slot_size(partitions: Path) -> int:
    with partitions.open(encoding="utf-8") as handle:
        rows = csv.reader(line for line in handle if not line.lstrip().startswith("#"))
        for row in rows:
            if len(row) >= 5 and row[0].strip() == "app0":
                return int(row[4].strip(), 0)
    raise ValueError(f"app0 partition missing in {partitions}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--environment", default="xiao-esp32c6-aws")
    parser.add_argument("--partitions", type=Path, default=C6 / "partitions_4mb.csv")
    parser.add_argument("--max-percent", type=float, default=85.0)
    args = parser.parse_args()

    binary = C6 / ".pio" / "build" / args.environment / "firmware.bin"
    if not binary.exists():
        parser.error(f"build artifact missing: {binary}")
    slot = app_slot_size(args.partitions)
    used = binary.stat().st_size
    percent = used * 100.0 / slot
    limit = int(slot * args.max_percent / 100.0)
    print(
        f"{args.environment}: binary={used} slot={slot} "
        f"usage={percent:.2f}% gate={args.max_percent:.2f}% headroom={limit - used}"
    )
    return 0 if used <= limit else 1


if __name__ == "__main__":
    raise SystemExit(main())
