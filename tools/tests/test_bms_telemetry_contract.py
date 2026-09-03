from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class BmsTelemetryContractTests(unittest.TestCase):
    def test_confirmed_pioneer_decoder_scale_and_filters_are_present(self) -> None:
        source = (ROOT / "firmware/common/decoders/decoder_standard_can_bms.h").read_text()
        pioneer = (ROOT / "firmware/common/decoders/decoder_standard_can_v1_pioneer.cpp").read_text()
        self.assertIn("PIONEER_RULES", pioneer)
        self.assertIn("0.3f,     // confirmed amperes per raw unit", pioneer)
        self.assertIn("12000.0f", pioneer)
        self.assertIn("25000.0f", pioneer)
        self.assertIn("data[6] == 0x20", source)
        self.assertLess(
            source.index("telemetry.charging.plugged = telemetry.bms.plugged"),
            source.index("if (currentPlausible)"),
        )
        self.assertIn("frame.id == 0x18D", source)
        self.assertIn("telemetry.bms.vehiclePowerW = -powerW", source)
        self.assertIn("telemetry.bms.isRegenerating", source)
        self.assertIn("telemetry.bms.isDischarging", source)
        self.assertIn("frame.id == 0x48D", pioneer)
        self.assertIn("telemetry.bms.socInternal = frame.data[6]", pioneer)
        self.assertIn("telemetry.bms.socDisplay = frame.data[7]", pioneer)

    def test_v2_decoder_uses_large_battery_big_endian_layout(self) -> None:
        source = (ROOT / "firmware/common/decoders/decoder_standard_can_v2.cpp").read_text()
        self.assertIn("readBe16", source)
        self.assertIn("frame.id == 0x1B0", source)
        self.assertIn("frame.id == 0x1B1", source)
        self.assertIn("frame.id == 0x2BA", source)
        self.assertIn("socHundredths / 100.0f", source)
        self.assertIn("sohHundredths / 100.0f", source)
        self.assertIn("currentRaw / 10.0f", source)
        self.assertIn("telemetry.bms.vehiclePowerW = -powerW", source)
        self.assertNotIn("frame.id == 0x18D", source)
        self.assertNotIn("frame.id == 0x4AD", source)
        self.assertNotIn("PIONEER_RULES", source)

        frame_1b0 = bytes.fromhex("00 00 B8 D9 0E 34 0E 3A")
        self.assertEqual(47321, int.from_bytes(frame_1b0[2:4], "big"))
        self.assertEqual(3636, int.from_bytes(frame_1b0[4:6], "big"))
        self.assertEqual(3642, int.from_bytes(frame_1b0[6:8], "big"))
        frame_1b1 = bytes.fromhex("0C 8F 25 E5 00 00 0B AE")
        self.assertEqual(32.15, int.from_bytes(frame_1b1[0:2], "big") / 100)
        self.assertEqual(97.01, int.from_bytes(frame_1b1[2:4], "big") / 100)
        self.assertEqual(-2.0, int.from_bytes(bytes.fromhex("FF EC"), "big", signed=True) / 10)

    def test_aws_soc_remains_display_can_soc(self) -> None:
        source = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text()
        self.assertIn('publishFloat("display/soc", telemetry.display.soc, 1)', source)
        self.assertNotIn('publishFloat("display/soc", telemetry.bms.socPercent', source)
        self.assertIn("telemetry.bms.statusLastUpdateMs != 0", source)

    def test_standard_can_prevents_all_display_charging_overrides(self) -> None:
        configurations = (
            ROOT / "firmware/lilygo-t-a7670/src/can/lilygo_can.cpp",
            ROOT / "firmware/esp32-c6/src/c6_dual_can.cpp",
        )
        for path in configurations:
            with self.subTest(path=path):
                source = path.read_text()
                self.assertIn("standardCanConfigured", source)
                self.assertIn("DECODER_PROFILE_DISPLAY_CAN", source)
                self.assertIn("frame.id == 0x603 || frame.id == 0x604", source)
        lilygo = configurations[0].read_text()
        self.assertIn("suppressedDisplayCharging", lilygo)

    def test_all_device_publishers_use_the_same_bms_topics(self) -> None:
        publishers = (
            ROOT / "firmware/esp32-wroom/src/mqtt/mqtt_client.cpp",
            ROOT / "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp",
            ROOT / "firmware/esp32-c6/src/c6_aws.cpp",
        )
        for path in publishers:
            source = path.read_text()
            with self.subTest(path=path):
                for topic in (
                    '"bms/pack_voltage"',
                    '"bms/pack_current"',
                    '"bms/pack_power_w"',
                    '"bms/vehicle_power_w"',
                    '"bms/is_regenerating"',
                    '"bms/is_discharging"',
                    '"bms/cell_min_mv"',
                    '"bms/cell_max_mv"',
                    '"bms/cell_delta_mv"',
                    '"bms/soc_internal"',
                    '"bms/soc_display"',
                    '"bms/standard_soc"',
                    '"bms/soh_percent"',
                ):
                    self.assertIn(topic, source)

    def test_portal_renders_pack_and_cell_values(self) -> None:
        source = (ROOT / "build/dashboard/current/js/app.js").read_text()
        for topic in (
            "bms/pack_voltage",
            "bms/pack_current",
            "bms/pack_power_w",
            "bms/vehicle_power_w",
            "bms/is_regenerating",
            "bms/is_discharging",
            "bms/cell_min_mv",
            "bms/cell_max_mv",
            "bms/cell_delta_mv",
            "bms/soc_internal",
            "bms/soc_display",
            "bms/standard_soc",
            "bms/soh_percent",
        ):
            self.assertIn(topic, source)
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        self.assertIn('id="bms-soc-details"', html)
        self.assertIn('id="bms-soc-internal-row" hidden', html)
        self.assertIn('id="bms-soc-display-row" hidden', html)
        self.assertIn('id="bms-standard-soc-row" hidden', html)
        self.assertIn('id="bms-soh-row" hidden', html)

    def test_compatibility_power_uses_vehicle_sign_convention(self) -> None:
        for path in (
            ROOT / "firmware/esp32-wroom/src/mqtt/mqtt_client.cpp",
            ROOT / "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp",
            ROOT / "firmware/esp32-c6/src/c6_aws.cpp",
        ):
            with self.subTest(path=path):
                source = path.read_text()
                self.assertIn("telemetry.bms.vehiclePowerW / 100.0f", source)

    def test_portal_displays_charging_power_as_positive_magnitude(self) -> None:
        source = (ROOT / "build/dashboard/current/js/app.js").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        self.assertIn("charging ? Math.abs(powerW) : powerW", source)
        self.assertIn("charging ? 'Ladeleistung' : 'Fahrzeugleistung'", source)
        self.assertIn('id="power-label"', html)
        self.assertIn('class="battery-values"', html)
        self.assertIn("chargingPowerMain.hidden", source)
        self.assertIn('id="charging-power-main"', html)
        self.assertIn('id="mobile-power-flow"', html)
        self.assertIn('id="mobile-vehicle-power"', html)
        self.assertIn("speed > 1", source)
        self.assertIn("classList.toggle('is-driving'", source)
        self.assertEqual(html.count('data-power-meter hidden'), 2)
        self.assertIn("charging ? 'charging' : regenerating ? 'regeneration' : 'consumption'", source)
        self.assertIn("mode === 'charging' ? 3.5 : 20", source)
        self.assertIn("powerKw <= 1.6 ? 'low' : powerKw <= 2.4 ? 'medium' : 'high'", source)
        self.assertIn("powerKw <= 5 ? 'low' : powerKw <= 10 ? 'medium' : 'high'", source)
        self.assertIn("powerKw <= 3 ? 'low' : powerKw <= 10 ? 'medium' : 'high'", source)
        self.assertIn("powerKw / scaleMax * 100", source)
        self.assertIn("p < -0.1 && !charging && !plugged", source)


if __name__ == "__main__":
    unittest.main()
