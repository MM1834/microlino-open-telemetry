from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "firmware/lilygo-t-a7670/src"
CONFIG = (BASE / "config/lilygo_config.cpp").read_text(encoding="utf-8")
NETWORK = (BASE / "network/lilygo_network.cpp").read_text(encoding="utf-8")
WEB = (BASE / "web/lilygo_web.cpp").read_text(encoding="utf-8")
MQTT = (BASE / "mqtt/lilygo_mqtt.cpp").read_text(encoding="utf-8")
MODEM = (BASE / "modem/lilygo_modem.cpp").read_text(encoding="utf-8")
AWS_H = (ROOT / "firmware/shared-libs/MotAwsIot/src/MotAwsIot.h").read_text(encoding="utf-8")


class LilygoLocalSecurityTests(unittest.TestCase):
    def test_admin_password_is_bounded_and_printable(self) -> None:
        self.assertIn("password.length() < 12", CONFIG)
        self.assertIn("password.length() > 63", CONFIG)
        self.assertIn("character < 32 || character > 126", CONFIG)

    def test_operational_ap_is_password_protected(self) -> None:
        self.assertIn("config.localAdminConfigured()", NETWORK)
        self.assertIn("WiFi.softAP(apSsid.c_str(), config.otaPassword.c_str())", NETWORK)

    def test_ota_is_opt_in_authenticated_and_same_origin(self) -> None:
        self.assertIn('prefs.putBool("otaEnabled", false)', CONFIG)
        self.assertIn("config.otaEnabled && requireAdmin() && requireSameOrigin()", WEB)
        self.assertNotIn("config.otaPassword.isEmpty()", WEB)

    def test_all_operational_routes_are_wrapped(self) -> None:
        self.assertIn('server.authenticate("admin", config.otaPassword.c_str())', WEB)
        for route in (
            '"/api/status"',
            '"/api/telemetry"',
            '"/api/lilygo/network"',
            '"/api/lilygo/modem"',
            '"/api/lilygo/can"',
            '"/api/lilygo/gps"',
        ):
            line = next(line for line in WEB.splitlines() if f"server.on({route}" in line)
            self.assertIn("requireAdmin()", line, route)

    def test_mutating_routes_require_same_origin(self) -> None:
        for route in (
            '"/factory-reset"',
            '"/api/config/import"',
            '"/api/onboarding/complete"',
            '"/api/lilygo/abrp/test"',
            '"/api/lilygo/lte/mqtt-trace/clear"',
        ):
            line = next(line for line in WEB.splitlines() if f"server.on({route}" in line)
            self.assertIn("requireSameOrigin()", line, route)

    def test_secret_values_are_not_echoed_or_exported(self) -> None:
        self.assertNotIn('value=\'" + config.wifiPass', WEB)
        self.assertNotIn('value=\'" + config.mqttPass', WEB)
        self.assertNotIn('value=\'" + config.abrpApiKey', WEB)
        self.assertIn("lilygoConfigManager.exportJson(false)", WEB)
        self.assertIn("if (includeSecrets)", CONFIG)

    def test_aws_transport_prefers_wifi_and_falls_back_to_lte_tls(self) -> None:
        wifi = MQTT.index("WiFi.status() == WL_CONNECTED")
        lte = MQTT.index("lilygoGprsConnected()")
        self.assertLess(wifi, lte)
        self.assertIn("lilygoConfigureAwsTlsClient", MQTT)
        self.assertIn("awsClient.begin(awsCredentials, *secureClient)", MQTT)
        self.assertIn("downloadCertificate", MODEM)
        self.assertIn("setClientPrivateKey", MODEM)
        self.assertIn("Client& transportClient", AWS_H)


if __name__ == "__main__":
    unittest.main()
