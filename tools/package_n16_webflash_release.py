#!/usr/bin/env python3
"""Package one immutable, configuration-preserving C6 Web Flasher release."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import shutil


ROOT = Path(__file__).resolve().parents[1]
C6 = ROOT / "firmware" / "esp32-c6"
DEFAULT_BINARY = C6 / ".pio" / "build" / "nanoesp32c6-n16" / "firmware.bin"
DEFAULT_PARTITIONS = C6 / "partitions_16mb.csv"
DEFAULT_VERSION = ROOT / "firmware" / "common" / "system" / "version.h"
TARGET = "nanoesp32c6-n16"
CHIP_FAMILY = "ESP32-C6"
FLASH_SIZE = 16 * 1024 * 1024
APPLICATION_OFFSET = 0x10000
PROFILES = {
    TARGET: (DEFAULT_BINARY, DEFAULT_PARTITIONS, FLASH_SIZE),
    "xiao-esp32c6": (
        C6 / ".pio" / "build" / "xiao-esp32c6" / "firmware.bin",
        C6 / "partitions_4mb.csv",
        4 * 1024 * 1024,
    ),
}


def partition(partitions: Path, name: str) -> tuple[int, int]:
    with partitions.open(encoding="utf-8") as handle:
        rows = csv.reader(line for line in handle if not line.lstrip().startswith("#"))
        for row in rows:
            if len(row) >= 5 and row[0].strip() == name:
                return int(row[3].strip(), 0), int(row[4].strip(), 0)
    raise ValueError(f"{name} partition missing in {partitions}")


def firmware_version(version_header: Path) -> str:
    source = version_header.read_text(encoding="utf-8")
    sprint = re.search(r'^#define MOT_SPRINT "([^"]+)"$', source, re.MULTILINE)
    revision = re.search(r'^#define MOT_REVISION "([^"]+)"$', source, re.MULTILINE)
    if not sprint or not revision:
        raise ValueError("MOT_SPRINT or MOT_REVISION missing")
    return f"{sprint.group(1)}-{revision.group(1)}-AWS"


def build_manifest(binary: Path, partitions: Path, version_header: Path, target: str = TARGET) -> dict:
    if target not in PROFILES:
        raise ValueError(f"unsupported Web Flasher target: {target}")
    if binary.name != "firmware.bin" or binary.name.endswith("factory.bin"):
        raise ValueError("only the PlatformIO application firmware.bin is allowed")
    if not binary.is_file():
        raise ValueError(f"firmware binary missing: {binary}")
    offset, slot_size = partition(partitions, "app0")
    if offset != APPLICATION_OFFSET:
        raise ValueError(f"unexpected C6 application offset: 0x{offset:x}")
    size = binary.stat().st_size
    if size <= 0 or size > slot_size:
        raise ValueError(f"application size {size} exceeds slot {slot_size}")
    version = firmware_version(version_header)
    binary_bytes = binary.read_bytes()
    if version.encode("ascii") not in binary_bytes:
        raise ValueError(
            f"application binary does not contain expected firmware version {version}"
        )
    digest = hashlib.sha256(binary_bytes).hexdigest()
    flash_size = PROFILES[target][2]
    artifact_name = f"mot-{target}-{version.lower()}-{digest[:12]}.bin"
    return {
        "schemaVersion": 1,
        "target": target,
        "version": version,
        "chipFamily": CHIP_FAMILY,
        "flashSizeBytes": flash_size,
        "writePlan": [{"offset": APPLICATION_OFFSET, "size": size}],
        "artifact": {"file": artifact_name, "size": size, "sha256": digest},
        "preserves": ["nvs", "otadata", "littlefs", "awsCredentials"],
        "factoryErase": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=sorted(PROFILES), default=TARGET)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--partitions", type=Path)
    parser.add_argument("--version-header", type=Path, default=DEFAULT_VERSION)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    default_binary, default_partitions, _flash_size = PROFILES[args.target]
    binary = args.binary or default_binary
    partitions = args.partitions or default_partitions
    manifest = build_manifest(binary, partitions, args.version_header, args.target)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    artifact = args.output_dir / manifest["artifact"]["file"]
    shutil.copyfile(binary, artifact)
    manifest_path = args.output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"manifest={manifest_path} artifact={artifact} sha256={manifest['artifact']['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
