from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
DISPLAY = (ROOT / "firmware/mot-display-c6/src/main.cpp").read_text(encoding="utf-8")
PROTOCOL = (ROOT / "firmware/common/display/mot_display_ble_protocol.h").read_text(encoding="utf-8")
CLIENT = (ROOT / "firmware/mot-display-c6/src/mot_ble_client.cpp").read_text(encoding="utf-8")
SERVER = (ROOT / "firmware/esp32-c6/src/c6_display_ble.cpp").read_text(encoding="utf-8")
GESTURE = (ROOT / "firmware/mot-display-c6/src/mot_gesture.cpp").read_text(encoding="utf-8")
ADAPTER_MAIN = (ROOT / "firmware/esp32-c6/src/main.cpp").read_text(encoding="utf-8")
VERSION = (ROOT / "firmware/common/system/version.h").read_text(encoding="utf-8")
ADAPTER_INI = (ROOT / "firmware/esp32-c6/platformio.ini").read_text(encoding="utf-8")


class MotDisplayContractTests(unittest.TestCase):
    def test_charging_is_positive_only_at_the_rendering_boundary(self):
        self.assertIn(
            "sample.state == FlowState::Charging\n      ? fabsf(sample.powerKw) : sample.powerKw",
            DISPLAY,
        )
        self.assertIn('snprintf(power, sizeof(power), "%+.1f", displayedPowerKw)', DISPLAY)
        self.assertIn("sample.powerKw = packet.powerDeciKw / 10.0F", DISPLAY)

    def test_protocol_v1_sign_and_layout_are_unchanged(self):
        self.assertIn("int16_t powerDeciKw", PROTOCOL)
        self.assertIn("sizeof(TelemetryV1) == 16", PROTOCOL)
        self.assertIn("constexpr uint8_t kProtocolVersion = 1", PROTOCOL)

    def test_capabilities_are_additive_and_required_before_subscription(self):
        self.assertIn("sizeof(CapabilitiesV1) == 10", PROTOCOL)
        self.assertIn("sizeof(ExtendedTelemetryV1) == 32", PROTOCOL)
        self.assertIn("sizeof(DrivingStatusV1) == 14", PROTOCOL)
        self.assertIn("kExtendedTelemetryV1Feature", PROTOCOL)
        self.assertIn("kDrivingStatusV1Feature", PROTOCOL)
        self.assertIn("kCapabilitiesUuid", CLIENT)
        self.assertIn("MotDisplayBle::valid(capabilities)", CLIENT)
        self.assertIn("kRequiredFeatures", CLIENT)
        self.assertIn("kCapabilitiesUuid", SERVER)

    def test_link_is_bonded_and_bound_to_one_adapter(self):
        self.assertIn("setAuthenticationMode(true, false, true)", CLIENT)
        self.assertIn("setAuthenticationMode(true, false, true)", SERVER)
        self.assertIn("client->secureConnection()", CLIENT)
        self.assertIn("ESP_GATT_PERM_READ_ENCRYPTED", SERVER)
        self.assertIn("preferences.putString(kPeerAddressKey", CLIENT)
        self.assertIn("equalsIgnoreCase(boundPeerAddress)", CLIENT)

    def test_ble_scan_does_not_block_the_display_loop(self):
        self.assertIn("scanner->start(kScanDurationSeconds, scanComplete, false)", CLIENT)
        self.assertNotIn("scanner->start(kScanDurationSeconds, false)", CLIENT)
        self.assertIn("void scanComplete(BLEScanResults)", CLIENT)

    def test_conflicting_flow_state_does_not_render_as_valid_power(self):
        self.assertIn("flowFlags & (flowFlags - 1U)", DISPLAY)
        self.assertIn("sample.powerValid = false", DISPLAY)

    def test_driving_status_is_additive_and_uses_display_can(self):
        decoder = (ROOT / "firmware/common/decoders/decoder_display_can.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("telemetry.display.speedKmh = b[1] / 2.0f", decoder)
        self.assertIn("hazardsActive = (b[2] & 0x08U) != 0", decoder)
        self.assertIn("leftIndicatorActive = hazardsActive || (b[2] & 0x04U) != 0", decoder)
        self.assertIn("rightIndicatorActive = hazardsActive || (b[2] & 0x02U) != 0", decoder)
        dual_can = (ROOT / "firmware/esp32-c6/src/c6_dual_can.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("decoderDisplayCanHandleLocalStatusFrame(frame)", dual_can)
        self.assertIn("Canonical charging and power", dual_can)
        self.assertIn("kDrivingStatusUuid", CLIENT)
        self.assertIn("motBleClientLatestDriving", DISPLAY)
        self.assertIn('? String(static_cast<unsigned>(lroundf(driving.speedDeciKmh / 10.0F))) : "--"', DISPLAY)
        self.assertIn('printCentered("km/h"', DISPLAY)
        self.assertIn("kLeftIndicatorActive", DISPLAY)
        self.assertIn("kRightIndicatorActive", DISPLAY)

    def test_display_adapter_build_supports_standard_can_v2(self):
        self.assertIn('value == "v2"', ADAPTER_MAIN)
        self.assertIn("DECODER_PROFILE_STANDARD_CAN_V2", ADAPTER_MAIN)

    def test_display_stream_alias_uses_the_production_identity(self):
        self.assertIn('MOT_VERSION "C6-001-REV41-AWS"', VERSION)
        display_env = ADAPTER_INI.split("[env:nanoesp32c6-n16-display-ble]", 1)[1]
        display_env = display_env.split("[env:", 1)[0]
        self.assertIn("extends = env:nanoesp32c6-n16", display_env)
        self.assertNotIn("MOT_DISPLAY_STREAM_BUILD", display_env)

    def test_indicator_state_stays_out_of_cloud_and_local_mqtt(self):
        aws = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text(encoding="utf-8")
        mqtt = (ROOT / "firmware/esp32-c6/src/c6_local_mqtt.cpp").read_text(encoding="utf-8")
        for field in (
            "indicatorStateValid",
            "leftIndicatorActive",
            "rightIndicatorActive",
            "safetyStateValid",
            "parkingBrakeActive",
            "warning90Code",
            "warningHblAhActive",
            "drivetrainWarningYellow",
        ):
            self.assertNotIn(field, aws)
            self.assertNotIn(field, mqtt)

    def test_local_display_safety_states_are_decoded_and_rendered(self):
        decoder = (ROOT / "firmware/common/decoders/decoder_display_can.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("parkingBrakeActive = (b[2] & 0x10U) != 0", decoder)
        self.assertIn("warning90Code = b[3]", decoder)
        self.assertIn("warningHblAhActive = (b[5] & 0x08U) != 0", decoder)
        self.assertIn("drivetrainWarningYellow = (b[5] & 0x20U) != 0", decoder)
        self.assertIn("kParkingBrakeActive", SERVER)
        self.assertIn("warning90Code != 0U", SERVER)
        self.assertIn("kWarning90Active", SERVER)
        self.assertIn("kWarningHblAhActive", SERVER)
        self.assertIn("kDrivetrainWarningYellow", SERVER)
        self.assertIn('graphics->print("P")', DISPLAY)
        self.assertIn('"! WARNUNG 90"', DISPLAY)
        self.assertIn('"! HBL / AH"', DISPLAY)
        self.assertIn('"! ANTRIEB GELB"', DISPLAY)

    def test_gesture_uses_the_qmi8658_hardware_tap_engine(self):
        self.assertIn("kConfigureTapCommand = 0x0C", GESTURE)
        self.assertIn("writeRegister(kCtrl2, 0x25)", GESTURE)
        self.assertIn("kTapStatus = 0x59", GESTURE)
        self.assertIn("hardware tap count=%u", GESTURE)

    def test_tilt_sampling_retains_stale_sample_protection(self):
        self.assertIn("kSampleIntervalMs = 4", GESTURE)
        self.assertIn("kRawJerkThreshold = 400", GESTURE)
        self.assertIn("kStaleSampleMs = 50", GESTURE)

    def test_page_navigation_uses_a_held_relative_tilt(self):
        self.assertIn("kTiltCalibrationMs = 1500", GESTURE)
        self.assertIn("kTiltHoldMs = 700", GESTURE)
        self.assertIn("kServiceTiltHoldMs = 3000", GESTURE)
        self.assertIn("kTiltTriggerDegrees = 35.0F", GESTURE)
        self.assertIn("kTiltReleaseDegrees = 15.0F", GESTURE)
        self.assertIn("atan2f(filteredY, filteredX)", GESTURE)
        self.assertIn("MotGestureEvent::TiltRight", DISPLAY)
        self.assertIn("MotGestureEvent::TiltLeft", DISPLAY)
        self.assertNotIn("MotGestureEvent::DoubleKnock", DISPLAY)

    def test_pairing_reset_requires_the_status_page_and_confirmation(self):
        loop = DISPLAY.split("void loop()", 1)[1]
        self.assertIn("pairingGestureContext && !pairingConfirmationPending", loop)
        self.assertIn("pairingGestureContext && pairingConfirmationPending", loop)
        self.assertIn("gesture != pairingConfirmationDirection", loop)
        self.assertIn("motBleClientForgetPairing();", loop)
        self.assertIn("kPairingConfirmationWindowMs = 10000", DISPLAY)
        self.assertNotIn("TripleKnock", loop)

    def test_pairing_reset_is_advertised_only_on_the_status_page(self):
        self.assertIn('motGestureAvailable() ? "3s DREHEN = KOPPELN"', DISPLAY)
        self.assertIn('printCentered("3s GEGENRICHTUNG"', DISPLAY)
        self.assertNotIn('"2X SEITE / 3X NEU KOPPELN"', DISPLAY)

    def test_long_ble_footer_uses_smaller_text(self):
        self.assertIn("footer.length() > 16 ? 1 : 2", DISPLAY)
        self.assertIn("printCentered(footer, 184", DISPLAY)
        self.assertIn('kFirmwareVersion[] = "MOT-DISPLAY-1.1.0"', DISPLAY)

    def test_aws_health_is_additive_and_publish_based(self):
        aws = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text(encoding="utf-8")
        self.assertIn("kAwsRecentlyHealthy = 1U << 1", PROTOCOL)
        self.assertIn("AWS_RECENTLY_HEALTHY_MS = 30000", aws)
        self.assertIn("lastSuccessfulPublishMs = millis()", aws)
        self.assertIn("c6AwsRecentlyHealthy()", SERVER)
        self.assertIn('awsRecentlyHealthy ? "ZULETZT OK"', DISPLAY)

    def test_pages_follow_vehicle_state_and_return_home(self):
        self.assertIn("constexpr uint32_t kPageReturnMs = 15000", DISPLAY)
        self.assertIn("constexpr uint32_t kStatusPageReturnMs = 30000", DISPLAY)
        self.assertIn("return chargingHome ? DisplayPage::Charging : DisplayPage::Power", DISPLAY)
        self.assertIn("case DisplayPage::Power: return DisplayPage::Journey", DISPLAY)
        self.assertIn("case DisplayPage::Charging: return DisplayPage::Battery", DISPLAY)
        self.assertIn("nowMs - lastPageSelectionMs >= pageReturnMs", DISPLAY)

    def test_pairing_confirmation_requires_a_neutral_release(self):
        self.assertIn("tiltNeedsRelease = true", GESTURE)
        self.assertIn("fabsf(deltaDegrees) <= kTiltReleaseDegrees", GESTURE)

    def test_adapter_prioritizes_ble_around_bounded_cloud_work(self):
        loop = ADAPTER_MAIN.split("void loop()", 1)[1]
        self.assertGreaterEqual(loop.count("c6DisplayBleLoop();"), 2)
        self.assertIn("c6CloudWorkerLoop();", loop)
        self.assertIn("#ifdef MOT_DISPLAY_BLE", loop)
        self.assertIn('"Loop stall: service=aws durationMs=%lu', loop)
        self.assertIn('"Loop stall: service=local-mqtt durationMs=%lu', loop)

    def test_display_adapter_runs_blocking_cloud_clients_off_the_main_loop(self):
        worker = (ROOT / "firmware/esp32-c6/src/c6_cloud_worker.cpp").read_text(encoding="utf-8")
        self.assertIn('xTaskCreate(cloudWorker, "mot-cloud", 12288', worker)
        self.assertIn("c6AwsLoop();", worker)
        self.assertIn("c6LocalMqttLoop();", worker)
        self.assertIn("Cloud worker busy:", worker)

    def test_cloud_dns_and_abrp_tls_are_serialized_safely(self):
        aws = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text(encoding="utf-8")
        mqtt = (ROOT / "firmware/esp32-c6/src/c6_local_mqtt.cpp").read_text(encoding="utf-8")
        worker = (ROOT / "firmware/esp32-c6/src/c6_cloud_worker.cpp").read_text(encoding="utf-8")
        resolved = (ROOT / "firmware/common/network/resolved_network_client_secure.h").read_text(encoding="utf-8")
        self.assertIn("resolveEndpointOnLoopTask()", aws)
        self.assertIn("resolveOnLoopTask()", mqtt)
        self.assertIn("NetworkClientSecure::connect(", resolved)
        self.assertIn("hostname_.c_str()", resolved)
        self.assertIn("c6AwsDisconnect();", worker)
        self.assertIn("c6LocalMqttDisconnect();", worker)

    def test_extended_pages_use_only_available_adapter_values(self):
        for label in ("TRAKTIONSBATTERIE", "AKTUELLE FAHRT", "LADEN", "ZELLDIFFERENZ"):
            self.assertIn(label, DISPLAY)
        self.assertIn("c6JourneyDisplaySnapshot()", SERVER)
        self.assertIn("c6ChargeDisplaySnapshot()", SERVER)
        self.assertIn("kChargeTargetValid", DISPLAY)
        self.assertIn("kChargeRemainingValid", DISPLAY)
        self.assertIn('"RESTZEIT " + durationText(packet.chargeRemainingMinutes)', DISPLAY)
        self.assertIn('sample.socValid ? "SOC " + String(sample.socPercent)', DISPLAY)


if __name__ == "__main__":
    unittest.main()
