import csv
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class LilygoDualCanContractTests(unittest.TestCase):
    def test_partition_retains_two_large_ota_slots(self):
        path = ROOT / "firmware/lilygo-t-a7670/partitions_4mb.csv"
        with path.open(encoding="utf-8") as handle:
            rows = {row[0].strip(): [cell.strip() for cell in row]
                    for row in csv.reader(line for line in handle if not line.startswith("#"))}
        self.assertEqual(rows["app0"][4], "0x1A0000")
        self.assertEqual(rows["app1"][4], "0x1A0000")
        self.assertEqual(rows["spiffs"][4], "0xA0000")

    def test_build_declares_mcp2515_and_partition(self):
        source = (ROOT / "firmware/lilygo-t-a7670/platformio.ini").read_text(encoding="utf-8")
        self.assertIn("board_build.partitions = partitions_4mb.csv", source)
        self.assertIn("autowp/autowp-mcp2515 @ ^1.3.1", source)

    def test_can2_is_independently_configured_and_listen_only(self):
        source = (ROOT / "firmware/lilygo-t-a7670/src/can/lilygo_can.cpp").read_text(encoding="utf-8")
        self.assertIn("setListenOnlyMode", source)
        self.assertIn("config.can2Profile", source)
        self.assertIn("config.canProfile", source)
        self.assertIn("MAX_FRAMES_PER_CHANNEL_PER_LOOP", source)
        self.assertNotIn("sendMessage", source)

    def test_can2_miso_avoids_esp32_strapping_pin(self):
        source = (ROOT / "firmware/lilygo-t-a7670/include/board_config.h").read_text(encoding="utf-8")
        self.assertIn("#define CAN2_SPI_MISO_PIN 39", source)
        self.assertNotIn("#define CAN2_SPI_MISO_PIN 2", source)

    def test_can2_mosi_avoids_esp32_strapping_pin(self):
        source = (ROOT / "firmware/lilygo-t-a7670/include/board_config.h").read_text(encoding="utf-8")
        self.assertIn("#define CAN_RX_PIN 36", source)
        self.assertIn("#define CAN2_SPI_MOSI_PIN 32", source)
        self.assertNotIn("#define CAN2_SPI_MOSI_PIN 15", source)

    def test_mcp2515_is_not_constructed_before_arduino_setup(self):
        source = (ROOT / "firmware/lilygo-t-a7670/src/can/lilygo_can.cpp").read_text(encoding="utf-8")
        self.assertIn("MCP2515* can2Controller = nullptr", source)
        self.assertIn("can2Controller = new MCP2515", source)

    def test_can2_config_survives_backup_restore(self):
        source = (ROOT / "firmware/lilygo-t-a7670/src/config/lilygo_config.cpp").read_text(encoding="utf-8")
        self.assertGreaterEqual(source.count("ConfigKeys::CAN2_PROFILE"), 2)
        web = (ROOT / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp").read_text(encoding="utf-8")
        self.assertIn("name='can2Profile'", web)
        self.assertIn("config.can2Profile", web)

    def test_default_mapping_matches_c6(self):
        header = (ROOT / "firmware/lilygo-t-a7670/src/config/lilygo_config.h").read_text(encoding="utf-8")
        self.assertIn("canProfile = DECODER_PROFILE_STANDARD_CAN_V1_PIONEER", header)
        self.assertIn("can2Profile = DECODER_PROFILE_DISPLAY_CAN", header)
        source = (ROOT / "firmware/lilygo-t-a7670/src/config/lilygo_config.cpp").read_text(encoding="utf-8")
        self.assertIn('prefs.putBool("dualCanMapV1", true)', source)

    def test_hardware_interlocks_are_documented(self):
        source = (ROOT / "docs/project/sprints/LG-CAN2-001.md").read_text(encoding="utf-8")
        self.assertIn("`TERM` must be electrically open", source)
        self.assertIn("`SLNT` must be tied to 3.3 V", source)
        self.assertIn("| RST | 3.3 V |", source)

    def test_cache_is_bounded_default_off_and_uses_ack_replay(self):
        header = (ROOT / "firmware/lilygo-t-a7670/src/config/lilygo_config.h").read_text(encoding="utf-8")
        cache = (ROOT / "firmware/lilygo-t-a7670/src/cache/lilygo_offline_cache.cpp").read_text(encoding="utf-8")
        mqtt = (ROOT / "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp").read_text(encoding="utf-8")
        self.assertIn("offlineCacheEnabled = false", header)
        self.assertIn("constexpr size_t MAX_BYTES = 131072", cache)
        self.assertIn('client.publish("history/backfill/v1"', cache)
        self.assertIn('client.subscribe("history/backfill/ack/v1"', cache)
        self.assertIn("freshLivePublished", cache)
        self.assertIn("lilygoOfflineCacheHandleAwsMessage", mqtt)

    def test_lilygo_uses_isolated_vehicle_identity(self):
        header = (ROOT / "firmware/lilygo-t-a7670/src/config/lilygo_config.h").read_text(encoding="utf-8")
        source = (ROOT / "firmware/lilygo-t-a7670/src/config/lilygo_config.cpp").read_text(encoding="utf-8")
        device = (ROOT / "private/aws/mot-lilygo-fe8ce0/device.json").read_text(encoding="utf-8")
        self.assertIn('vehicleId = "pioneer-lilygo"', header)
        self.assertIn('if (config.vehicleId == "pioneer") config.vehicleId = "pioneer-lilygo"', source)
        self.assertIn('\"vehicleId\": \"pioneer-lilygo\"', device)


if __name__ == "__main__":
    unittest.main()
