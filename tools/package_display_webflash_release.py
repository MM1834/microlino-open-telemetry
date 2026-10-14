#!/usr/bin/env python3
"""Package an immutable full-install MOT Display WebFlash release."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "firmware" / "mot-display-c6" / ".pio" / "build" / "waveshare-esp32-c6-lcd-128"
DEFAULT_FACTORY = BUILD / "firmware.factory.bin"
DEFAULT_BOOTLOADER = BUILD / "bootloader.bin"
DEFAULT_PARTITIONS = BUILD / "partitions.bin"
DEFAULT_APPLICATION = BUILD / "firmware.bin"
TARGET = "waveshare-esp32-c6-lcd-128-b"
BOARD_MODEL = "Waveshare ESP32-C6-LCD-1.28-B (SKU 35451)"
CHIP_FAMILY = "ESP32-C6"
FLASH_SIZE = 16 * 1024 * 1024
BOOTLOADER_OFFSET = 0x0
PARTITIONS_OFFSET = 0x8000
APPLICATION_OFFSET = 0x10000
FACTORY_OFFSET = 0x0
LAYOUT_MARKER_OFFSET = 0x8C00
LAYOUT_MARKER_SIZE = 128
LAYOUT_MARKER_MAGIC = b"MOTDISPLAYLAYOUT"
LAYOUT_SCHEMA_VERSION = 1
VERSION_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")


def _bytes(path: Path, label: str) -> bytes:
    if not path.is_file():
        raise ValueError(f"{label} missing: {path}")
    value = path.read_bytes()
    if not value:
        raise ValueError(f"{label} is empty: {path}")
    return value


def build_layout_marker(partition_bytes: bytes) -> bytes:
    partition_digest = hashlib.sha256(partition_bytes).digest()
    target_bytes = TARGET.encode("ascii")
    if len(target_bytes) > 63:
        raise ValueError("display target is too long for layout marker")
    marker = bytearray([0xFF] * LAYOUT_MARKER_SIZE)
    marker[:len(LAYOUT_MARKER_MAGIC)] = LAYOUT_MARKER_MAGIC
    struct.pack_into("<I", marker, 16, LAYOUT_SCHEMA_VERSION)
    marker[20] = len(target_bytes)
    marker[21:21 + len(target_bytes)] = target_bytes
    marker[64:96] = partition_digest
    marker[96:128] = hashlib.sha256(marker[:96]).digest()
    return bytes(marker)


def build_factory_image(factory_bytes: bytes, partition_bytes: bytes) -> bytes:
    marker = build_layout_marker(partition_bytes)
    result = bytearray(factory_bytes)
    marker_end = LAYOUT_MARKER_OFFSET + len(marker)
    if len(result) < marker_end:
        result.extend(b"\xFF" * (marker_end - len(result)))
    if any(value != 0xFF for value in result[LAYOUT_MARKER_OFFSET:marker_end]):
        raise ValueError("layout marker region is not blank")
    result[LAYOUT_MARKER_OFFSET:marker_end] = marker
    return bytes(result)


def build_manifest(
    factory: Path,
    bootloader: Path,
    partitions: Path,
    application: Path,
    version: str,
    version_marker: str,
) -> dict:
    if not VERSION_RE.fullmatch(version):
        raise ValueError("invalid display release version")
    if factory.name != "firmware.factory.bin":
        raise ValueError("only PlatformIO firmware.factory.bin is allowed")
    factory_bytes = _bytes(factory, "factory image")
    bootloader_bytes = _bytes(bootloader, "bootloader")
    partition_bytes = _bytes(partitions, "partition table")
    application_bytes = _bytes(application, "application")
    if factory_bytes[0] != 0xE9 or bootloader_bytes[0] != 0xE9:
        raise ValueError("invalid ESP image header")
    if len(factory_bytes) > FLASH_SIZE:
        raise ValueError("factory image exceeds physical flash")
    if factory_bytes[BOOTLOADER_OFFSET:len(bootloader_bytes)] != bootloader_bytes:
        raise ValueError("factory image does not contain the reviewed bootloader")
    if factory_bytes[PARTITIONS_OFFSET:PARTITIONS_OFFSET + len(partition_bytes)] != partition_bytes:
        raise ValueError("factory image does not contain the reviewed partition table")
    if factory_bytes[APPLICATION_OFFSET:APPLICATION_OFFSET + len(application_bytes)] != application_bytes:
        raise ValueError("factory image does not contain the reviewed display application")
    marker = version_marker.encode("utf-8")
    if not marker or marker not in application_bytes:
        raise ValueError("display application does not contain the expected version marker")
    partition_digest = hashlib.sha256(partition_bytes).digest()
    marker_payload = build_layout_marker(partition_bytes)
    factory_bytes = build_factory_image(factory_bytes, partition_bytes)
    digest = hashlib.sha256(factory_bytes).hexdigest()
    artifact_name = f"mot-{TARGET}-{version.lower()}-{digest[:12]}.bin"
    return {
        "schemaVersion": 2,
        "target": TARGET,
        "boardModel": BOARD_MODEL,
        "version": version,
        "chipFamily": CHIP_FAMILY,
        "flashSizeBytes": FLASH_SIZE,
        "installMode": "factory-install",
        "writePlan": [{"offset": FACTORY_OFFSET, "size": len(factory_bytes)}],
        "layoutMarker": {
            "offset": LAYOUT_MARKER_OFFSET,
            "size": LAYOUT_MARKER_SIZE,
            "schemaVersion": LAYOUT_SCHEMA_VERSION,
            "sha256": hashlib.sha256(marker_payload).hexdigest(),
            "partitionTableSha256": partition_digest.hex(),
        },
        "artifact": {"file": artifact_name, "size": len(factory_bytes), "sha256": digest},
        "replaces": ["vendor-demo", "bootloader", "partition-table", "application"],
        "factoryErase": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--factory", type=Path, default=DEFAULT_FACTORY)
    parser.add_argument("--bootloader", type=Path, default=DEFAULT_BOOTLOADER)
    parser.add_argument("--partitions", type=Path, default=DEFAULT_PARTITIONS)
    parser.add_argument("--application", type=Path, default=DEFAULT_APPLICATION)
    parser.add_argument("--version", required=True)
    parser.add_argument("--version-marker", default="MOT-DISPLAY-1.2.0-RC6")
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    manifest = build_manifest(
        args.factory, args.bootloader, args.partitions, args.application,
        args.version, args.version_marker,
    )
    args.output_dir.mkdir(parents=True, exist_ok=True)
    artifact = args.output_dir / manifest["artifact"]["file"]
    artifact.write_bytes(build_factory_image(args.factory.read_bytes(), args.partitions.read_bytes()))
    manifest_path = artifact.with_suffix(".json")
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(manifest_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
