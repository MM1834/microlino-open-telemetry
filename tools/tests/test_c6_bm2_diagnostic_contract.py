from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
PIO = (ROOT / "firmware/esp32-c6/platformio.ini").read_text()
VERSION = (ROOT / "firmware/common/system/version.h").read_text()
SOURCE = (ROOT / "firmware/esp32-c6/src/c6_bm2_diag.cpp").read_text()
WEB = (ROOT / "firmware/esp32-c6/src/c6_web.cpp").read_text()


class C6Bm2DiagnosticContractTest(unittest.TestCase):
    def test_bm2_is_a_separate_n16_diagnostic_build(self):
        self.assertIn("[env:nanoesp32c6-n16-bm2-diagnostic]", PIO)
        self.assertIn("-D MOT_BM2_DIAGNOSTIC=1", PIO)
        xiao = PIO.split("[env:xiao-esp32c6]", 1)[1]
        self.assertNotIn("MOT_BM2_DIAGNOSTIC", xiao)
        self.assertIn('MOT_VERSION "C6-001-REV38-BM2-BM6-AUTO-DIAG"', VERSION)

    def test_bm2_diagnostic_is_bounded_and_query_only(self):
        self.assertIn("setActiveScan(true)", SOURCE)
        self.assertIn("kScanDurationSeconds = 15", SOURCE)
        self.assertIn("kScanIntervalMs = 60000", SOURCE)
        self.assertIn("mbedtls_aes_crypt_cbc", SOURCE)
        self.assertIn("BLEDevice::createClient()", SOURCE)
        self.assertIn("kBm2NotifyUuid", SOURCE)
        self.assertIn("decoded[0] != 0xF5", SOURCE)
        self.assertIn("notify->subscribe", SOURCE)
        self.assertIn("c6Bm2DiagRequestHistory", SOURCE)
        self.assertIn("const uint8_t request[] = {0xE7, 0x01}", SOURCE)
        self.assertIn("0xE3, 0x00, 0x00", SOURCE)
        self.assertIn("c6Bm6DiagConnect", SOURCE)
        self.assertIn("0xD1, 0x55, 0x07", SOURCE)
        self.assertIn("kBm6Key", SOURCE)
        self.assertIn("lastTemperatureC", SOURCE)

    def test_local_ui_auto_diagnostic_is_bounded(self):
        self.assertIn("kAutoCaptureTimeoutMs = 60000", SOURCE)
        self.assertIn("kAutoMaximumConnectionAttempts = 3", SOURCE)
        self.assertIn("autoDiagnostic", SOURCE)
        self.assertIn("exclusiveConnectionRequired", SOURCE)
        self.assertIn("c6Bm2DiagRequestHistory()", SOURCE)
        self.assertIn("BM6 connected; collecting live values", SOURCE)
        self.assertIn("Battery monitor diagnostics", WEB)
        self.assertIn("d.bm2", WEB)


if __name__ == "__main__":
    unittest.main()
