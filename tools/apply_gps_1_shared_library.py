#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def ensure_line_after(text: str, anchor: str, line: str) -> str:
    if line in text:
        return text
    if anchor not in text:
        raise RuntimeError(f"Anchor not found: {anchor!r}")
    return text.replace(anchor, anchor + line, 1)

def update_esp32_platformio() -> None:
    path = ROOT / "firmware/esp32-wroom/platformio.ini"
    text = path.read_text(encoding="utf-8")

    if "lib_extra_dirs = ../shared-libs" not in text:
        text = ensure_line_after(
            text,
            "monitor_speed = 115200\n",
            "lib_extra_dirs = ../shared-libs\n"
        )

    if "mikalhart/TinyGPSPlus" not in text:
        text = text.replace(
            "lib_deps =\n",
            "lib_deps =\n  mikalhart/TinyGPSPlus @ ^1.0.3\n",
            1
        )

    if "[env:esp32dev-gps-test]" not in text:
        text += """

[env:esp32dev-gps-test]
extends = env:esp32dev
build_src_filter =
  -<*>
  +<gps/gps_test_main.cpp>
build_flags =
  -D MOT_GPS_RX_PIN=16
  -D MOT_GPS_TX_PIN=17
  -D MOT_GPS_BAUD=9600
"""

    path.write_text(text, encoding="utf-8")
    print(f"updated {path.relative_to(ROOT)}")

def update_lilygo_platformio() -> None:
    path = ROOT / "firmware/lilygo-t-a7670/platformio.ini"
    if not path.exists():
        return

    text = path.read_text(encoding="utf-8")
    if "lib_extra_dirs = ../shared-libs" not in text:
        text = ensure_line_after(
            text,
            "monitor_speed = 115200\n",
            "lib_extra_dirs = ../shared-libs\n"
        )

    if "mikalhart/TinyGPSPlus" not in text:
        text = text.replace(
            "lib_deps =\n",
            "lib_deps =\n  mikalhart/TinyGPSPlus @ ^1.0.3\n",
            1
        )

    path.write_text(text, encoding="utf-8")
    print(f"updated {path.relative_to(ROOT)}")

def main() -> None:
    update_esp32_platformio()
    update_lilygo_platformio()
    print("GPS-1 shared library and ESP32 test target are ready.")

if __name__ == "__main__":
    main()
