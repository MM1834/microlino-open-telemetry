import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.package_n16_webflash_release import (
    APPLICATION_OFFSET,
    CHIP_FAMILY,
    FLASH_SIZE,
    TARGET,
    build_manifest,
    main,
)


class N16WebflashReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.binary = self.root / "firmware.bin"
        self.binary.write_bytes(b"application-image")
        self.partitions = self.root / "partitions.csv"
        self.partitions.write_text(
            "nvs,data,nvs,0x9000,0x5000,\n"
            "otadata,data,ota,0xE000,0x2000,\n"
            "app0,app,ota_0,0x10000,0x500000,\n"
            "spiffs,data,spiffs,0xA10000,0x5E0000,\n",
            encoding="utf-8",
        )
        self.version = self.root / "version.h"
        self.version.write_text(
            '#define MOT_SPRINT "C6-001"\n#define MOT_REVISION "REV14"\n',
            encoding="utf-8",
        )

    def tearDown(self):
        self.temp.cleanup()

    def test_manifest_is_fixed_to_configuration_preserving_n16_application(self):
        manifest = build_manifest(self.binary, self.partitions, self.version)
        self.assertEqual(TARGET, manifest["target"])
        self.assertEqual(CHIP_FAMILY, manifest["chipFamily"])
        self.assertEqual(FLASH_SIZE, manifest["flashSizeBytes"])
        self.assertEqual(APPLICATION_OFFSET, manifest["writePlan"][0]["offset"])
        self.assertFalse(manifest["factoryErase"])
        self.assertEqual(
            hashlib.sha256(b"application-image").hexdigest(),
            manifest["artifact"]["sha256"],
        )
        self.assertIn("awsCredentials", manifest["preserves"])

    def test_factory_image_is_rejected(self):
        factory = self.root / "firmware.factory.bin"
        factory.write_bytes(b"factory")
        with self.assertRaisesRegex(ValueError, "application firmware.bin"):
            build_manifest(factory, self.partitions, self.version)

    def test_xiao_manifest_uses_four_mb_geometry(self):
        manifest = build_manifest(
            self.binary, self.partitions, self.version, "xiao-esp32c6"
        )
        self.assertEqual("xiao-esp32c6", manifest["target"])
        self.assertEqual(4 * 1024 * 1024, manifest["flashSizeBytes"])

    def test_wrong_offset_and_oversized_application_are_rejected(self):
        self.partitions.write_text("app0,app,ota_0,0x20000,0x10,\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "unexpected C6 application offset"):
            build_manifest(self.binary, self.partitions, self.version)
        self.partitions.write_text("app0,app,ota_0,0x10000,0x4,\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "exceeds slot"):
            build_manifest(self.binary, self.partitions, self.version)


if __name__ == "__main__":
    unittest.main()
