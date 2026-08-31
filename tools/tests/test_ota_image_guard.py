from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
GUARD = (ROOT / "firmware/common/web/ota_image_guard.h").read_text(encoding="utf-8")
SHARED = (ROOT / "firmware/common/web/local_ota.cpp").read_text(encoding="utf-8")
LILYGO = (ROOT / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp").read_text(encoding="utf-8")


class OtaImageGuardTests(unittest.TestCase):
    def test_guard_reads_standard_image_header(self):
        self.assertIn("data[0] != ESP_IMAGE_MAGIC", GUARD)
        self.assertIn("data[12]", GUARD)
        self.assertIn("data[13]", GUARD)
        self.assertIn("data[3] >> 4", GUARD)

    def test_guard_compares_chip_and_physical_flash(self):
        self.assertIn("defined(CONFIG_IDF_TARGET_ESP32C6)", GUARD)
        self.assertNotIn("CONFIG_IDF_TARGET_ESP32_C6", GUARD)
        self.assertIn('#error "Unsupported ESP target for OTA image guard"', GUARD)
        self.assertIn("imageChipId != runningChipId", GUARD)
        self.assertIn("ESP.getFlashChipSize()", GUARD)
        self.assertIn("imageFlashBytes != runningFlashBytes", GUARD)

    def test_shared_ota_validates_before_opening_partition(self):
        validation = SHARED.index("otaValidateImageHeader")
        begin = SHARED.index("Update.begin", validation)
        write = SHARED.index("Update.write", begin)
        self.assertLess(validation, begin)
        self.assertLess(begin, write)
        self.assertIn("The running firmware was not changed", SHARED)

    def test_lilygo_validates_before_opening_partition(self):
        upload = LILYGO.index("static void handleOtaUpload()")
        validation = LILYGO.index("otaValidateImageHeader", upload)
        begin = LILYGO.index("Update.begin", validation)
        write = LILYGO.index("Update.write", begin)
        self.assertLess(validation, begin)
        self.assertLess(begin, write)


if __name__ == "__main__":
    unittest.main()
