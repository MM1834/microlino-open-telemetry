import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class CanListenOnlyContractTests(unittest.TestCase):
    def test_all_maintained_can_targets_are_listen_only(self):
        sources = (
            "firmware/esp32-c6/src/c6_dual_can.cpp",
            "firmware/esp32-wroom/src/can/can_input.cpp",
            "firmware/lilygo-t-a7670/src/can/lilygo_can.cpp",
        )
        for source_path in sources:
            source = (ROOT / source_path).read_text(encoding="utf-8")
            self.assertIn("TWAI_MODE_LISTEN_ONLY", source, source_path)
            self.assertNotIn("TWAI_MODE_NORMAL", source, source_path)

    def test_no_application_twai_transmit_call_exists(self):
        firmware = ROOT / "firmware"
        for source_path in firmware.rglob("*.cpp"):
            if ".pio" not in source_path.parts:
                self.assertNotIn("twai_transmit", source_path.read_text(encoding="utf-8"), str(source_path))


if __name__ == "__main__":
    unittest.main()
