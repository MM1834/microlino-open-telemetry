from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
CONFIG = (ROOT / "firmware/esp32-wroom/src/app_config.cpp").read_text(encoding="utf-8")
NETWORK = (ROOT / "firmware/esp32-wroom/src/network/wifi_manager.cpp").read_text(encoding="utf-8")
OTA = (ROOT / "firmware/esp32-wroom/src/ota/ota_web.cpp").read_text(encoding="utf-8")
WEB = (ROOT / "firmware/esp32-wroom/src/web/web_ui.cpp").read_text(encoding="utf-8")
SHARED_OTA = (ROOT / "firmware/common/web/local_ota.cpp").read_text(encoding="utf-8")
SHARED_SECURITY = (ROOT / "firmware/common/web/local_web_security.cpp").read_text(encoding="utf-8")
MQTT = (ROOT / "firmware/esp32-wroom/src/mqtt/mqtt_client.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "firmware/esp32-wroom/src/main.cpp").read_text(encoding="utf-8")


class WroomLocalSecurityTests(unittest.TestCase):
    def test_physical_admin_recovery_preserves_other_configuration(self) -> None:
        self.assertIn('equalsIgnoreCase("admin recover")', MAIN)
        self.assertIn("LocalWebSecurity::generateRecoveryPassword()", MAIN)
        self.assertIn("appConfigManager.save()", MAIN)

    def test_admin_password_is_bounded_and_printable(self) -> None:
        self.assertIn("password.length() < 12", CONFIG)
        self.assertIn("password.length() > 63", CONFIG)
        self.assertIn("character < 32 || character > 126", CONFIG)

    def test_provisioned_fallback_ap_uses_password(self) -> None:
        self.assertIn("config.localAdminConfigured()", NETWORK)
        self.assertIn("WiFi.softAP(ssid.c_str(), config.otaPassword.c_str())", NETWORK)

    def test_ota_fails_closed_and_checks_origin(self) -> None:
        self.assertIn("options.enabled = config.otaEnabled", OTA)
        self.assertIn("!settings->enabled", SHARED_OTA)
        self.assertIn("LocalWebSecurity::requireSameOrigin", SHARED_OTA)
        self.assertNotIn("password.isEmpty()", SHARED_OTA)

    def test_operational_web_routes_require_authentication(self) -> None:
        self.assertIn("LocalWebSecurity::authenticate(server, config.otaPassword)", WEB)
        self.assertIn('server.authenticate("admin", password.c_str())', SHARED_SECURITY)
        for handler in (
            "handleStatus",
            "handleConfig",
            "handleSave",
            "handleConfigExport",
            "handleConfigImport",
            "handleFactoryReset",
            "handleApiConfigImport",
        ):
            marker = f"static void {handler}()"
            section = WEB.split(marker, 1)[1].split("\n}", 1)[0]
            self.assertIn("requireAdmin()", section, handler)

    def test_mutating_routes_require_same_origin(self) -> None:
        for handler in (
            "handleSetupSave",
            "handleOnboardingComplete",
            "handleOnboardingRestart",
            "handleSave",
            "handleAbrpTest",
            "handleConfigImport",
            "handleFactoryReset",
            "handleApiConfigImport",
        ):
            marker = f"static void {handler}()"
            section = WEB.split(marker, 1)[1].split("\n}", 1)[0]
            self.assertIn("requireSameOrigin()", section, handler)

    def test_secret_values_are_not_echoed_or_exported(self) -> None:
        self.assertNotIn('value=\'" + config.wifiPass', WEB)
        self.assertNotIn('value=\'" + config.mqttPass', WEB)
        self.assertNotIn('value=\'" + config.abrpApiKey', WEB)
        self.assertNotIn('value=\'" + config.abrpUserToken', WEB)
        self.assertIn("appConfigManager.exportJson(false)", WEB)
        self.assertIn("if (includeSecrets) {", CONFIG)

    def test_import_cannot_remove_local_admin_boundary(self) -> None:
        self.assertIn("if (!config.localAdminConfigured())", CONFIG)
        self.assertIn('error = "local admin password must be 12-63 characters"', CONFIG)

    def test_system_health_uses_active_transport(self) -> None:
        self.assertIn("mqttTransportDiagnostics()", WEB)
        self.assertNotIn('runMqttDiagnostics("mot-health")', WEB)
        self.assertIn('result.mode = "AWS_IOT_X509"', MQTT)
        self.assertIn("result.mqttOk = connected", MQTT)
        self.assertIn("transport=aws?'AWS IoT':'Legacy MQTT'", WEB)

    def test_gps_uart_noise_is_not_reported_as_valid_nmea(self) -> None:
        self.assertIn("UNVALIDATED / NOISE POSSIBLE", WEB)
        self.assertIn("GPS state", WEB)
        self.assertIn("g.detected?'DETECTED':'NOT DETECTED'", WEB)


if __name__ == "__main__":
    unittest.main()
