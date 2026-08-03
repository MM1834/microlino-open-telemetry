from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
CONFIG = (ROOT / "firmware/esp32-wroom/src/app_config.cpp").read_text(encoding="utf-8")
NETWORK = (ROOT / "firmware/esp32-wroom/src/network/wifi_manager.cpp").read_text(encoding="utf-8")
OTA = (ROOT / "firmware/esp32-wroom/src/ota/ota_web.cpp").read_text(encoding="utf-8")
WEB = (ROOT / "firmware/esp32-wroom/src/web/web_ui.cpp").read_text(encoding="utf-8")


class WroomLocalSecurityTests(unittest.TestCase):
    def test_admin_password_is_bounded_and_printable(self) -> None:
        self.assertIn("password.length() < 12", CONFIG)
        self.assertIn("password.length() > 63", CONFIG)
        self.assertIn("character < 32 || character > 126", CONFIG)

    def test_provisioned_fallback_ap_uses_password(self) -> None:
        self.assertIn("config.localAdminConfigured()", NETWORK)
        self.assertIn("WiFi.softAP(ssid.c_str(), config.otaPassword.c_str())", NETWORK)

    def test_ota_fails_closed_and_checks_origin(self) -> None:
        self.assertIn("!config.otaEnabled || !config.localAdminConfigured()", OTA)
        self.assertIn("requireOtaAuth() && requireOtaSameOrigin()", OTA)
        self.assertNotIn("if (config.otaPassword.isEmpty()) {\n        return true;", OTA)

    def test_operational_web_routes_require_authentication(self) -> None:
        self.assertIn('server.authenticate("admin", config.otaPassword.c_str())', WEB)
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


if __name__ == "__main__":
    unittest.main()
