#!/usr/bin/env python3
"""Package a marker-gated MOT Display application-only WebFlash release."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "mot-display-c6" / ".pio" / "build" / "waveshare-esp32-c6-lcd-128"
DEFAULT_APPLICATION = BUILD / "firmware.bin"
DEFAULT_PARTITIONS = BUILD / "partitions.bin"
TARGET = "waveshare-esp32-c6-lcd-128-b-update"
BASE_TARGET = "waveshare-esp32-c6-lcd-128-b"
BOARD_MODEL = "Waveshare ESP32-C6-LCD-1.28-B (SKU 35451)"
FLASH_SIZE = 16 * 1024 * 1024
APPLICATION_OFFSET = 0x10000
MARKER_OFFSET = 0x8C00
MARKER_SIZE = 128
MARKER_SHA256 = "007908ae4fbba301897798e8aa8a377a6907d429ab133ba144e7c404d8c81623"
VERSION_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")


def build_manifest(application: Path, partitions: Path, version: str) -> dict:
    if not VERSION_RE.fullmatch(version):
        raise ValueError("invalid display update version")
    app = application.read_bytes()
    partition = partitions.read_bytes()
    if not app or app[0] != 0xE9:
        raise ValueError("invalid ESP application image")
    if version.encode() not in app:
        raise ValueError("application does not contain its release version")
    if APPLICATION_OFFSET + len(app) > FLASH_SIZE:
        raise ValueError("application exceeds display flash")
    digest = hashlib.sha256(app).hexdigest()
    partition_digest = hashlib.sha256(partition).hexdigest()
    name = f"mot-{TARGET}-{version.lower()}-{digest[:12]}.bin"
    return {
        "schemaVersion": 2,
        "target": TARGET,
        "baseTarget": BASE_TARGET,
        "boardModel": BOARD_MODEL,
        "version": version,
        "chipFamily": "ESP32-C6",
        "flashSizeBytes": FLASH_SIZE,
        "installMode": "application-update",
        "writePlan": [{"offset": APPLICATION_OFFSET, "size": len(app)}],
        "artifact": {"file": name, "size": len(app), "sha256": digest},
        "requiredLayoutMarker": {
            "offset": MARKER_OFFSET,
            "size": MARKER_SIZE,
            "schemaVersion": 1,
            "target": BASE_TARGET,
            "sha256": MARKER_SHA256,
            "partitionTableOffset": 0x8000,
            "partitionTableSize": len(partition),
            "partitionTableSha256": partition_digest,
        },
        "factoryErase": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--application", type=Path, default=DEFAULT_APPLICATION)
    parser.add_argument("--partitions", type=Path, default=DEFAULT_PARTITIONS)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    manifest = build_manifest(args.application, args.partitions, args.version)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    artifact = args.output_dir / manifest["artifact"]["file"]
    shutil.copyfile(args.application, artifact)
    manifest_path = artifact.with_suffix(".json")
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(manifest_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
