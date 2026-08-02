from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest


TOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIR))

from upload_aws_credentials import select_environment, validate_device_metadata


class EnvironmentValidationTests(unittest.TestCase):
    def test_defaults_to_board_aws_environment(self) -> None:
        self.assertEqual(select_environment("esp32-wroom", None), "esp32dev-aws")
        self.assertEqual(
            select_environment("lilygo-t-a7670", None),
            "T-A7670X-AWS",
        )

    def test_rejects_non_aws_or_other_board_environment(self) -> None:
        with self.assertRaises(ValueError):
            select_environment("esp32-wroom", "esp32dev")
        with self.assertRaises(ValueError):
            select_environment("esp32-wroom", "T-A7670X-AWS")


class MetadataValidationTests(unittest.TestCase):
    def test_accepts_matching_thing_name(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / "device.json").write_text(
                json.dumps({"thingName": "mot-test-device"})
            )
            validate_device_metadata(source, "mot-test-device")

    def test_rejects_mismatched_thing_name(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / "device.json").write_text(
                json.dumps({"thingName": "mot-other-device"})
            )
            with self.assertRaises(ValueError):
                validate_device_metadata(source, "mot-test-device")

    def test_rejects_invalid_json(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / "device.json").write_text("not-json")
            with self.assertRaises(ValueError):
                validate_device_metadata(source, "mot-test-device")


if __name__ == "__main__":
    unittest.main()
