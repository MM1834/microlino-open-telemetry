#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
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

AWS_ENVIRONMENTS = {
    "esp32-wroom": "esp32dev-aws",
    "lilygo-t-a7670": "T-A7670X-AWS",
}


def select_environment(firmware: str, requested: str | None) -> str:
    expected = AWS_ENVIRONMENTS[firmware]
    environment = requested or expected
    if environment != expected:
        raise ValueError(
            f"Environment {environment!r} is not the AWS provisioning target "
            f"for {firmware!r}; expected {expected!r}"
        )
    return environment


def validate_device_metadata(source: Path, thing_name: str) -> None:
    try:
        metadata = json.loads((source / "device.json").read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError("device.json is missing, unreadable or invalid") from exc

    if metadata.get("thingName") != thing_name:
        raise ValueError("device.json thingName does not match requested Thing")


def validate_upload_port(value: str) -> str:
    port = value.strip()
    if not port.startswith(("/dev/cu.", "/dev/tty.")):
        raise ValueError("upload port must be an explicit macOS serial device")
    return port

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
    parser.add_argument(
        "--upload-port",
        required=True,
        help="Exact serial device, for example /dev/cu.usbserial-0001",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    source = root / "private/aws" / args.thing_name
    project = root / "firmware" / args.firmware
    destination = project / "data/aws"

    try:
        env = select_environment(args.firmware, args.environment)
        upload_port = validate_upload_port(args.upload_port)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 2

    missing = [name for name in FILES if not (source / name).is_file()]
    if missing:
        print("Missing credential files:", ", ".join(missing))
        return 1

    try:
        validate_device_metadata(source, args.thing_name)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 2

    destination.mkdir(parents=True, exist_ok=True)

    try:
        for name in FILES:
            shutil.copy2(source / name, destination / name)

        subprocess.run(
            [
                "pio", "run", "-e", env, "-t", "uploadfs",
                "--upload-port", upload_port,
            ],
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
