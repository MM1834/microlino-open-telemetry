import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "package_display_webflash_update.py"
SPEC = importlib.util.spec_from_file_location("display_update_packager", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DisplayWebflashUpdateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        root = Path(self.temp.name)
        self.application = root / "firmware.bin"
        self.partitions = root / "partitions.bin"
        self.version = "MOT-DISPLAY-0.2-PRE3"
        self.application.write_bytes(b"\xe9APP " + self.version.encode() + b"\x00" * 64)
        self.partitions.write_bytes(b"partition-table" + b"\xff" * (0xC00 - 15))

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_manifest_is_application_only_and_marker_gated(self) -> None:
        manifest = MODULE.build_manifest(self.application, self.partitions, self.version)
        self.assertEqual("waveshare-esp32-c6-lcd-128-b-update", manifest["target"])
        self.assertEqual("application-update", manifest["installMode"])
        self.assertEqual(0x10000, manifest["writePlan"][0]["offset"])
        self.assertFalse(manifest["factoryErase"])
        marker = manifest["requiredLayoutMarker"]
        self.assertEqual(0x8C00, marker["offset"])
        self.assertEqual(128, marker["size"])
        self.assertEqual("waveshare-esp32-c6-lcd-128-b", marker["target"])
        self.assertEqual(0x8000, marker["partitionTableOffset"])
        self.assertEqual(0xC00, marker["partitionTableSize"])

    def test_invalid_application_header_fails(self) -> None:
        self.application.write_bytes(b"BAD " + self.version.encode())
        with self.assertRaisesRegex(ValueError, "invalid ESP application"):
            MODULE.build_manifest(self.application, self.partitions, self.version)

    def test_missing_embedded_version_fails(self) -> None:
        self.application.write_bytes(b"\xe9APP other-version")
        with self.assertRaisesRegex(ValueError, "does not contain"):
            MODULE.build_manifest(self.application, self.partitions, self.version)

    def test_application_must_fit_flash(self) -> None:
        original = MODULE.FLASH_SIZE
        MODULE.FLASH_SIZE = MODULE.APPLICATION_OFFSET + 4
        try:
            with self.assertRaisesRegex(ValueError, "exceeds display flash"):
                MODULE.build_manifest(self.application, self.partitions, self.version)
        finally:
            MODULE.FLASH_SIZE = original


if __name__ == "__main__":
    unittest.main()
