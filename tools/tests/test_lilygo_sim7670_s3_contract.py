import csv
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "firmware/lilygo-t-a7670"


class LilygoSim7670S3ContractTests(unittest.TestCase):
    def test_build_target_and_storage_layout(self):
        ini = (BASE / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("[env:T-SIM7670G-S3-Standard-AWS]", ini)
        self.assertIn("board = esp32-s3-devkitc1-n16r2", ini)
        self.assertIn("-D TINY_GSM_MODEM_SIM7670G", ini)
        self.assertIn("board_build.partitions = partitions_16mb.csv", ini)
        self.assertIn("-D MOT_OFFLINE_CACHE_MAX_BYTES=262144", ini)

        with (BASE / "partitions_16mb.csv").open(encoding="utf-8") as handle:
            rows = {row[0].strip(): [cell.strip() for cell in row]
                    for row in csv.reader(line for line in handle if not line.startswith("#"))}
        self.assertEqual(rows["app0"][4], "0x500000")
        self.assertEqual(rows["app1"][4], "0x500000")
        self.assertEqual(rows["spiffs"][4], "0x5E0000")

    def test_board_pin_contract(self):
        board = (BASE / "include/board_config.h").read_text(encoding="utf-8")
        expected = (
            "#define MODEM_TX_PIN 4", "#define MODEM_RX_PIN 5",
            "#define MODEM_POWERON_PULSE_WIDTH_MS 100",
            "#define GPS_RX_PIN 48", "#define GPS_TX_PIN 45",
            "#define CAN_RX_PIN 39", "#define CAN_TX_PIN 40",
            "#define CAN2_SPI_SCK_PIN 12", "#define CAN2_SPI_MOSI_PIN 11",
            "#define CAN2_SPI_MISO_PIN 13", "#define CAN2_SPI_CS_PIN 10",
            "#define CAN2_INT_PIN 14",
        )
        for declaration in expected:
            self.assertIn(declaration, board)

    def test_tls_and_gnss_have_sim7670_paths(self):
        modem = (BASE / "src/modem/lilygo_modem.cpp").read_text(encoding="utf-8")
        gps = (BASE / "src/gps/l76k_gps.cpp").read_text(encoding="utf-8")
        self.assertIn("modem.fs_write", modem)
        self.assertIn("bool responding = modem.testAT(1000)", modem)
        self.assertIn("deadline = millis() + 15000", modem)
        self.assertIn("lteSecureClient.setClientPrivateKey", modem)
        self.assertIn("modem.enableNMEA(false)", modem)
        self.assertIn("RETRY_INTERVAL_MS = 30000", gps)

    def test_new_board_has_isolated_fresh_identity(self):
        board = (BASE / "include/board_config.h").read_text(encoding="utf-8")
        self.assertIn('#define LILYGO_DEFAULT_DEVICE_PREFIX "mot-sim7670-"', board)
        self.assertIn('#define LILYGO_DEFAULT_VEHICLE_ID "pioneer-sim7670"', board)

    def test_s3_ota_guard_and_receive_only_can(self):
        guard = (ROOT / "firmware/common/web/ota_image_guard.h").read_text(encoding="utf-8")
        can = (BASE / "src/can/lilygo_can.cpp").read_text(encoding="utf-8")
        self.assertIn("defined(CONFIG_IDF_TARGET_ESP32S3)", guard)
        self.assertIn("return 9;", guard)
        self.assertIn("TWAI_MODE_LISTEN_ONLY", can)
        self.assertIn("setListenOnlyMode", can)
        self.assertNotIn("twai_transmit", can)
        self.assertNotIn("sendMessage", can)


if __name__ == "__main__":
    unittest.main()
