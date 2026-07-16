#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

FILES = (
    "AmazonRootCA1.pem",
    "device-certificate.pem.crt",
    "device-private-key.pem.key",
    "device.json",
    "policy.json",
)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("thing_name")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    source = root / "secrets/aws-iot" / args.thing_name
    destination = root / "private/aws" / args.thing_name

    if not source.is_dir():
        print(f"Missing source directory: {source}")
        return 1

    destination.mkdir(parents=True, exist_ok=True)
    for name in FILES:
        path = source / name
        if path.is_file():
            shutil.copy2(path, destination / name)

    print(destination)
    return 0

if __name__ == "__main__":
    sys.exit(main())
