#!/usr/bin/env python3
"""Stage one device's AWS credentials into LittleFS, upload, then clean them."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys


REQUIRED_FILES = (
    "AmazonRootCA1.pem",
    "device-certificate.pem.crt",
    "device-private-key.pem.key",
    "device.json",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("thing_name", help="AWS IoT Thing name")
    parser.add_argument(
        "--environment",
        default="T-A7670X-AWS",
        help="PlatformIO environment (default: T-A7670X-AWS)",
    )
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[1]
    source = repo / "private" / "aws" / args.thing_name
    firmware = repo / "firmware" / "lilygo-t-a7670"
    staging = firmware / "data" / "aws"

    missing = [name for name in REQUIRED_FILES if not (source / name).is_file()]
    if missing:
        print(f"Missing credentials in {source}: {', '.join(missing)}", file=sys.stderr)
        return 2

    metadata = json.loads((source / "device.json").read_text())
    if metadata.get("thingName") != args.thing_name:
        print("device.json thingName does not match requested Thing", file=sys.stderr)
        return 2

    staging.mkdir(parents=True, exist_ok=True)

    try:
        for name in REQUIRED_FILES:
            shutil.copy2(source / name, staging / name)

        subprocess.run(
            ["pio", "run", "-e", args.environment, "-t", "uploadfs"],
            cwd=firmware,
            check=True,
        )
    finally:
        for name in REQUIRED_FILES:
            target = staging / name
            if target.exists():
                target.unlink()

    print(f"Uploaded AWS credentials for {args.thing_name}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
