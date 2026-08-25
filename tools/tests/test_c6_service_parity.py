from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
C6 = ROOT / "firmware/esp32-c6/src"
CONFIG = (C6 / "c6_config.cpp").read_text(encoding="utf-8")
WEB = (C6 / "c6_web.cpp").read_text(encoding="utf-8")
MAIN = (C6 / "main.cpp").read_text(encoding="utf-8")
ABRP = (ROOT / "firmware/common/abrp/abrp_client.cpp").read_text(encoding="utf-8")
WROOM_ADAPTER = (ROOT / "firmware/esp32-wroom/src/abrp/wroom_abrp.cpp").read_text(encoding="utf-8")
AWS = (C6 / "c6_aws.cpp").read_text(encoding="utf-8")
AWS_CLIENT = (ROOT / "firmware/shared-libs/MotAwsIot/src/MotAwsIot.cpp").read_text(encoding="utf-8")


class C6ServiceParityTests(unittest.TestCase):
    def test_common_abrp_has_no_board_specific_dependencies(self) -> None:
        self.assertNotIn("app_config.h", ABRP)
        self.assertNotIn("wroom_gps.h", ABRP)
        self.assertIn("AbrpLocationProvider", (ROOT / "firmware/common/abrp/abrp_client.h").read_text())
        self.assertIn("setupAbrp(settings())", WROOM_ADAPTER)

    def test_abrp_send_is_off_main_loop(self) -> None:
        self.assertIn("xTaskCreate(sendTask", ABRP)
        self.assertIn("currentStatus.inFlight", ABRP)
        self.assertIn("c6AbrpLoop();", MAIN)
        self.assertIn("c6AwsLoop();", MAIN)

    def test_abrp_rejects_stale_wifi_and_bounds_connect(self) -> None:
        self.assertIn("WiFi.localIP() == IPAddress(0, 0, 0, 0)", ABRP)
        self.assertIn("http.setConnectTimeout(5000)", ABRP)

    def test_abrp_releases_tls_objects_before_task_deletion(self) -> None:
        self.assertIn("void performSend", ABRP)
        task = ABRP.split("void sendTask", 1)[1].split("\n}", 1)[0]
        self.assertLess(task.index("performSend(*input)"), task.index("vTaskDelete(nullptr)"))
        self.assertIn("MIN_FREE_HEAP_BYTES", ABRP)
        self.assertIn("lowMemorySkips", ABRP)

    def test_abrp_prefers_fresh_standard_can_charging_state(self) -> None:
        telemetry = (ROOT / "firmware/common/telemetry/telemetry.cpp").read_text(encoding="utf-8")
        lilygo_abrp = (ROOT / "firmware/lilygo-t-a7670/src/abrp/lilygo_abrp.cpp").read_text(encoding="utf-8")
        self.assertIn('addBool("is_charging", telemetryIsCharging())', ABRP)
        self.assertIn('"is_charging\\":" + String(telemetryIsCharging()', lilygo_abrp)
        self.assertIn("freshBmsStatus", telemetry)
        self.assertIn("telemetry.bms.plugged && telemetry.bms.packCurrentA > 2.0f", telemetry)
        self.assertIn(": telemetry.charging.isCharging", telemetry)

    def test_c6_abrp_yields_to_primary_aws_transport(self) -> None:
        c6_abrp = (C6 / "c6_abrp.cpp").read_text(encoding="utf-8")
        self.assertIn("c6AwsConnected()", c6_abrp)
        self.assertIn("c6NetworkTransportReady()", c6_abrp)

    def test_serial_recovery_can_disable_abrp_without_erasing_credentials(self) -> None:
        self.assertIn('normalized == "abrp disable"', MAIN)
        self.assertIn('normalized == "abrp send"', MAIN)
        self.assertIn("c6ConfigSetAbrpEnabled(enabled)", MAIN)
        setter = CONFIG.split("void c6ConfigSetAbrpEnabled", 1)[1].split("\n}", 1)[0]
        self.assertIn('preferences.putBool("abrpEn", enabled)', setter)
        self.assertNotIn("abrpApiKey", setter)
        self.assertNotIn("abrpUserToken", setter)

    def test_aws_connect_failure_is_bounded_and_backed_off(self) -> None:
        self.assertIn("secureClient_.setHandshakeTimeout(5)", AWS_CLIENT)
        self.assertIn("mqtt_.setSocketTimeout(5)", AWS_CLIENT)
        self.assertIn("MAX_RECONNECT_INTERVAL_MS", (ROOT / "firmware/shared-libs/MotAwsIot/src/MotAwsIot.h").read_text())
        self.assertIn("consecutiveConnectFailures", AWS_CLIENT)
        self.assertIn("totalConnectFailures", AWS_CLIENT)

    def test_c6_persists_service_state_but_redacts_secrets(self) -> None:
        self.assertIn('preferences.putBool("abrpEn"', CONFIG)
        self.assertIn('preferences.putString("abrpKey"', CONFIG)
        self.assertIn('doc["abrpEnabled"]', CONFIG)
        secret_section = CONFIG.split("String c6ConfigExportJson", 1)[1].split("bool c6ConfigImportJson", 1)[0]
        self.assertIn('if (includeSecrets)', secret_section)
        self.assertNotIn('doc["abrpApiKey"] = c6Config.abrpApiKey;\n    doc', secret_section)

    def test_authenticated_abrp_and_onboarding_routes_exist(self) -> None:
        for route in (
            '"/wizard"', '"/api/onboarding"', '"/api/onboarding/complete"',
            '"/api/onboarding/restart"', '"/api/abrp/status"', '"/api/abrp/test"',
        ):
            self.assertIn(route, WEB)
        self.assertIn("LocalWebSecurity::requireSameOrigin(server)", WEB)
        self.assertIn("if (!requireAdmin()) return", WEB)

    def test_existing_admin_installations_migrate_without_forced_wizard(self) -> None:
        self.assertIn('preferences.isKey("onboarded")', CONFIG)
        self.assertIn("validAdminPassword(c6Config.adminPassword)", CONFIG)
        self.assertIn("c6Config.onboardingComplete", WEB)

    def test_c6_wizard_progress_is_persisted_and_resumed(self) -> None:
        config_h = (C6 / "c6_config.h").read_text(encoding="utf-8")
        self.assertIn("uint8_t onboardingStep = 1", config_h)
        self.assertIn('preferences.getUChar(\n        "onboardStep"', CONFIG)
        self.assertIn('preferences.putUChar("onboardStep"', CONFIG)
        self.assertIn("c6Config.onboardingStep = 4", WEB)
        self.assertIn("c6Config.onboardingStep = 5", WEB)
        self.assertIn("c6Config.onboardingStep = 6", WEB)
        self.assertIn('server.on("/api/onboarding/step", HTTP_POST', WEB)

    def test_first_setup_transitions_once_to_admin_and_wizard(self) -> None:
        setup = WEB.split("void setupPage()", 1)[1].split("String channelJson", 1)[0]
        self.assertIn("one-time <b>setup</b> login will no longer work", setup)
        self.assertIn("sign in as <b>admin</b>", setup)
        self.assertNotIn("name='ssid'", setup)
        self.assertIn("Initial setup is already complete", setup)
        self.assertIn("name='adminPasswordConfirm'", setup)
        self.assertIn('server.arg("adminPassword") != server.arg("adminPasswordConfirm")', setup)
        self.assertLess(
            setup.index('server.arg("adminPassword") != server.arg("adminPasswordConfirm")'),
            setup.index('c6ConfigSetAdminPassword(server.arg("adminPassword"))'),
        )
        self.assertIn("The device was not changed", setup)
        self.assertIn("physical USB console recovery", setup)
        self.assertIn("using the new password", setup)
        self.assertIn("with the same new password", setup)

    def test_wizard_embeds_configuration_and_dynamic_handoff(self) -> None:
        self.assertIn('action=\'/wizard/connectivity\'', WEB)
        self.assertIn('action=\'/wizard/vehicle\'', WEB)
        self.assertIn('action=\'/wizard/services\'', WEB)
        self.assertIn("c6NetworkProfileName()", WEB)
        self.assertIn("c6NetworkIp()", WEB)
        self.assertIn("c6NetworkApSsid()", WEB)
        self.assertIn("http://192.168.4.1", WEB)
        self.assertNotIn("c6Config.wifiPassword +", WEB)
        self.assertNotIn("c6Config.wifi2Password +", WEB)

    def test_settings_steps_have_one_unambiguous_continue_action(self) -> None:
        self.assertIn("const bool settingsStep", WEB)
        self.assertIn("step == onboardingStepNumber(OnboardingStep::Connectivity)", WEB)
        self.assertIn("step == onboardingStepNumber(OnboardingStep::Vehicle)", WEB)
        self.assertIn("step == onboardingStepNumber(OnboardingStep::Services)", WEB)
        self.assertIn("Save WiFi &amp; continue", WEB)
        self.assertIn("Save CAN &amp; continue", WEB)
        self.assertNotIn("Save, reboot &amp; continue", WEB)

    def test_unchanged_can_wizard_settings_continue_without_reboot(self) -> None:
        handler = WEB.split("void wizardVehicleSave()", 1)[1].split("void wizardServicesSave()", 1)[0]
        self.assertIn("const bool profilesChanged", handler)
        self.assertIn("if (!profilesChanged)", handler)
        self.assertLess(handler.index("if (!profilesChanged)"), handler.index("scheduleReboot();"))
        self.assertIn('server.sendHeader("Location", "/wizard")', handler)
        self.assertIn("restarts only if a CAN profile was changed", WEB)

    def test_wizard_explains_connectivity_services_validation_and_ip_handoff(self) -> None:
        self.assertIn("Use a 2.4 GHz WiFi network", WEB)
        self.assertIn("Maximize Compatibility", WEB)
        self.assertLess(WEB.index("<h3>History cache</h3>"), WEB.index("<h3>ABRP (optional)</h3>"))
        self.assertIn("<hr><h3>ABRP (optional)</h3>", WEB)
        self.assertIn("Device and telemetry validation", WEB)
        self.assertIn("Start validation", WEB)
        self.assertIn("router may assign a different IP later", WEB)
        self.assertIn("also shown in the MOT Portal", WEB)

    def test_blank_littlefs_is_formatted_but_corruption_is_fail_closed(self) -> None:
        self.assertIn("littleFsPartitionIsErased", AWS_CLIENT)
        self.assertIn("esp_partition_read", AWS_CLIENT)
        self.assertIn("buffer[i] != 0xff", AWS_CLIENT)
        self.assertIn("return LittleFS.begin(true)", AWS_CLIENT)
        self.assertIn("return LittleFS.begin(false)", AWS_CLIENT)

    def test_unprovisioned_aws_runtime_is_fail_open_for_local_services(self) -> None:
        setup = AWS.split("void c6AwsSetup()", 1)[1].split("void c6AwsLoop()", 1)[0]
        self.assertIn("if (!motLoadAwsCredentials(credentials))", setup)
        self.assertIn("return;", setup)
        loop = AWS_CLIENT.split("void MotAwsIotClient::loop", 1)[1]
        self.assertIn("if (!enabled()) return;", loop)


if __name__ == "__main__":
    unittest.main()
