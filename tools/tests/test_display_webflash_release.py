from pathlib import Path
import hashlib
import tempfile
import unittest

from tools.package_display_webflash_release import (
    APPLICATION_OFFSET,
    LAYOUT_MARKER_MAGIC,
    LAYOUT_MARKER_OFFSET,
    LAYOUT_MARKER_SIZE,
    PARTITIONS_OFFSET,
    build_factory_image,
    build_layout_marker,
    build_manifest,
)


class DisplayWebflashReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.bootloader = self.root / "bootloader.bin"
        self.partitions = self.root / "partitions.bin"
        self.application = self.root / "firmware.bin"
        self.factory = self.root / "firmware.factory.bin"
        self.bootloader.write_bytes(b"\xe9BOOT")
        self.partitions.write_bytes(b"PARTITIONS")
        self.application.write_bytes(b"\xe9APP MOT-DISPLAY-0.2-PRE2")
        merged = bytearray(b"\xff" * (APPLICATION_OFFSET + self.application.stat().st_size))
        merged[:self.bootloader.stat().st_size] = self.bootloader.read_bytes()
        merged[PARTITIONS_OFFSET:PARTITIONS_OFFSET + self.partitions.stat().st_size] = self.partitions.read_bytes()
        merged[APPLICATION_OFFSET:] = self.application.read_bytes()
        self.factory.write_bytes(merged)

    def tearDown(self):
        self.temp.cleanup()

    def manifest(self):
        return build_manifest(
            self.factory, self.bootloader, self.partitions, self.application,
            "MOT-DISPLAY-0.2-PRE2", "MOT-DISPLAY-0.2-PRE2",
        )

    def test_full_install_manifest_is_fixed_to_supported_display(self):
        manifest = self.manifest()
        self.assertEqual("waveshare-esp32-c6-lcd-128-b", manifest["target"])
        self.assertEqual("factory-install", manifest["installMode"])
        self.assertEqual(max(self.factory.stat().st_size,
                             LAYOUT_MARKER_OFFSET + LAYOUT_MARKER_SIZE),
                         manifest["writePlan"][0]["size"])
        self.assertFalse(manifest["factoryErase"])
        self.assertEqual(64, len(manifest["artifact"]["sha256"]))
        self.assertEqual(LAYOUT_MARKER_OFFSET, manifest["layoutMarker"]["offset"])

    def test_factory_image_contains_self_checking_layout_marker(self):
        image = build_factory_image(self.factory.read_bytes(), self.partitions.read_bytes())
        marker = image[LAYOUT_MARKER_OFFSET:LAYOUT_MARKER_OFFSET + LAYOUT_MARKER_SIZE]
        self.assertEqual(LAYOUT_MARKER_MAGIC, marker[:len(LAYOUT_MARKER_MAGIC)])
        self.assertEqual(build_layout_marker(self.partitions.read_bytes()), marker)
        manifest = self.manifest()
        self.assertEqual(manifest["artifact"]["sha256"], hashlib.sha256(image).hexdigest())

    def test_rejects_nonblank_layout_marker_region(self):
        data = bytearray(self.factory.read_bytes())
        if len(data) <= LAYOUT_MARKER_OFFSET:
            data.extend(b"\xff" * (LAYOUT_MARKER_OFFSET + 1 - len(data)))
        data[LAYOUT_MARKER_OFFSET] = 0
        with self.assertRaisesRegex(ValueError, "marker region"):
            build_factory_image(bytes(data), self.partitions.read_bytes())

    def test_rejects_factory_image_with_different_partition_table(self):
        data = bytearray(self.factory.read_bytes())
        data[PARTITIONS_OFFSET] ^= 0x01
        self.factory.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "partition table"):
            self.manifest()

    def test_rejects_missing_embedded_display_marker(self):
        with self.assertRaisesRegex(ValueError, "version marker"):
            build_manifest(
                self.factory, self.bootloader, self.partitions, self.application,
                "MOT-DISPLAY-0.2-PRE2", "MOT Display release 1",
            )


if __name__ == "__main__":
    unittest.main()
