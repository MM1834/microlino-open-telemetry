#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
obsolete = [
    root / "firmware/lilygo-t-a7670/src/aws/aws_iot_credentials.cpp",
    root / "firmware/lilygo-t-a7670/src/aws/aws_iot_credentials.h",
]

for path in obsolete:
    if path.exists():
        path.unlink()
        print(f"removed {path.relative_to(root)}")

aws_dir = root / "firmware/lilygo-t-a7670/src/aws"
if aws_dir.exists() and not any(aws_dir.iterdir()):
    aws_dir.rmdir()
    print(f"removed {aws_dir.relative_to(root)}")
