import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]


class EcuSoftwareVersionsContractTest(unittest.TestCase):
    def test_passive_decoder_ids_and_layouts(self):
        display = (ROOT / "firmware/common/decoders/decoder_display_can.cpp").read_text()
        pioneer = (ROOT / "firmware/common/decoders/decoder_standard_can_v1_pioneer.cpp").read_text()
        self.assertIn("frame.id == 0x700", pioneer)
        self.assertIn("frame.id == 0x0AC", display)
        self.assertIn("case 0x604: decode604", display)
        self.assertIn("case 0x606: decode606", display)
        self.assertIn("ipcMajor = b[5]", display)
        self.assertIn("bduMajor = b[0]", display)
        self.assertNotIn("twai_transmit", display + pioneer)

    def test_cloud_and_local_mqtt_publish_all_versions(self):
        for relative in (
            "firmware/esp32-c6/src/c6_aws.cpp",
            "firmware/esp32-c6/src/c6_local_mqtt.cpp",
        ):
            source = (ROOT / relative).read_text()
            self.assertIn('vehicle/vcu_software_version', source)
            self.assertIn('vehicle/ipc_software_version', source)
            self.assertIn('vehicle/bdu_software_version', source)

    def test_dual_can_keeps_ipc_frame_in_display_decoder(self):
        source = (ROOT / "firmware/esp32-c6/src/c6_dual_can.cpp").read_text()
        self.assertIn("const bool displayChargingFrame = frame.id == 0x603;", source)
        self.assertNotIn(
            "const bool displayChargingFrame = frame.id == 0x603 || frame.id == 0x604;",
            source,
        )

    def test_aws_ingest_and_dashboard_contract(self):
        template = (ROOT / "cloud/aws/foundation/template.yaml").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        app = (ROOT / "build/dashboard/current/js/app.js").read_text()
        css = (ROOT / "build/dashboard/current/css/dashboard.css").read_text()
        for suffix in ("vcu", "ipc", "bdu"):
            topic = f'vehicle/{suffix}_software_version'
            self.assertIn(f'"{topic}": "string"', template)
            self.assertIn(topic, app)
            self.assertIn(f'id="vehicle-{suffix}-version"', html)
        self.assertIn("@media(max-width:600px)", css)

    def test_display_protocol_and_screen_are_additive(self):
        protocol = (ROOT / "firmware/common/display/mot_display_ble_protocol.h").read_text()
        adapter = (ROOT / "firmware/esp32-c6/src/c6_display_ble.cpp").read_text()
        display = (ROOT / "firmware/mot-display-c6/src/main.cpp").read_text()
        dualeye = (ROOT / "firmware/mot-display-c6/src/main_s3_dualeye_128.cpp").read_text()
        touch185 = (ROOT / "firmware/mot-display-c6/src/main_s3_185c.cpp").read_text()
        self.assertIn("struct SoftwareVersionsV1", protocol)
        self.assertIn("kSoftwareVersionsV1Feature", adapter)
        self.assertIn("DisplayPage::SoftwareVersions", display)
        self.assertIn("renderSoftwareVersions", display)
        self.assertIn("renderSoftwareVersions", dualeye)
        self.assertIn("updateSoftwareVersionsPage", touch185)

    def test_unconfirmed_temperatures_are_not_product_telemetry(self):
        telemetry = (ROOT / "firmware/common/telemetry/telemetry.h").read_text()
        aws = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text()
        self.assertNotIn("motorTemperature", telemetry)
        self.assertNotIn("temperature_motor", aws)


if __name__ == "__main__":
    unittest.main()
