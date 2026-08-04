from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "firmware/lilygo-t-a7670"
CONFIG_H = (BASE / "src/config/lilygo_config.h").read_text(encoding="utf-8")
CONFIG_CPP = (BASE / "src/config/lilygo_config.cpp").read_text(encoding="utf-8")
MODEM = (BASE / "src/modem/lilygo_modem.cpp").read_text(encoding="utf-8")
NETWORK = (BASE / "src/network/lilygo_network.cpp").read_text(encoding="utf-8")
MQTT = (BASE / "src/mqtt/lilygo_mqtt.cpp").read_text(encoding="utf-8")
WEB = (BASE / "src/web/lilygo_web.cpp").read_text(encoding="utf-8")


class LilygoLteResilienceTests(unittest.TestCase):
    def test_apn_is_runtime_configuration_not_provider_default(self) -> None:
        self.assertIn("String lteApn;", CONFIG_H)
        self.assertIn('getStringOrDefault("lteApn", "")', CONFIG_CPP)
        self.assertNotIn('gprs.swisscom.ch', CONFIG_H + CONFIG_CPP)
        self.assertIn('config.lteApn.isEmpty()', MODEM)

    def test_optional_apn_credentials_are_editable_without_secret_echo(self) -> None:
        self.assertIn('name=\'lteUser\'', WEB)
        self.assertIn('name=\'ltePass\' type=\'password\'', WEB)
        self.assertIn('if (!server.arg("ltePass").isEmpty())', WEB)
        self.assertNotIn('value=\'" + config.ltePass', WEB)

    def test_lte_reconnect_is_bounded_and_backed_off(self) -> None:
        self.assertIn("connectNetworkAndGprs(15000)", MODEM)
        self.assertNotIn("connectNetworkAndGprs(30000)", MODEM)
        self.assertIn("consecutiveConnectFailures >= 4", MODEM)
        self.assertIn("powerKeyPulse();", MODEM)
        self.assertIn("LTE_RETRY_INITIAL_MS = 15000", NETWORK)
        self.assertIn("LTE_RETRY_MAX_MS = 300000", NETWORK)
        self.assertIn("lteRetryIntervalMs * 2UL", NETWORK)

    def test_wifi_remains_preferred_for_aws_without_changing_identity(self) -> None:
        wifi = MQTT.index("WiFi.status() == WL_CONNECTED")
        lte = MQTT.index("lilygoGprsConnected()")
        self.assertLess(wifi, lte)
        self.assertIn("awsCredentials.thingName", MQTT)
        self.assertIn("awsCredentials.vehicleId", MQTT)
        self.assertIn("lilygoConfigureAwsTlsClient", MQTT)


if __name__ == "__main__":
    unittest.main()
