#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

FILES = (
    "AmazonRootCA1.pem",
    "device-certificate.pem.crt",
    "device-private-key.pem.key",
    "device.json",
)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("thing_name")
    parser.add_argument(
        "firmware",
        choices=("esp32-wroom", "lilygo-t-a7670")
    )
    parser.add_argument(
        "--environment",
        default=None
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    source = root / "private/aws" / args.thing_name
    project = root / "firmware" / args.firmware
    destination = project / "data/aws"

    env = args.environment
    if not env:
        env = (
            "esp32dev-aws"
            if args.firmware == "esp32-wroom"
            else "T-A7670X-AWS"
        )

    missing = [name for name in FILES if not (source / name).is_file()]
    if missing:
        print("Missing credential files:", ", ".join(missing))
        return 1

    destination.mkdir(parents=True, exist_ok=True)

    try:
        for name in FILES:
            shutil.copy2(source / name, destination / name)

        subprocess.run(
            ["pio", "run", "-e", env, "-t", "uploadfs"],
            cwd=project,
            check=True
        )
    finally:
        for name in FILES:
            target = destination / name
            if target.exists():
                target.unlink()

    return 0

if __name__ == "__main__":
    sys.exit(main())
