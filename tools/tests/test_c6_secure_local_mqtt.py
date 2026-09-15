from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
C6 = ROOT / "firmware/esp32-c6/src"
CLIENT = (C6 / "c6_local_mqtt.cpp").read_text(encoding="utf-8")
CONFIG = (C6 / "c6_config.cpp").read_text(encoding="utf-8")
CONFIG_H = (C6 / "c6_config.h").read_text(encoding="utf-8")
WEB = (C6 / "c6_web.cpp").read_text(encoding="utf-8")
MAIN = (C6 / "main.cpp").read_text(encoding="utf-8")


class C6SecureLocalMqttTests(unittest.TestCase):
    def test_service_defaults_off_and_is_independent(self) -> None:
        self.assertIn("bool localMqttEnabled = false", CONFIG_H)
        self.assertIn('preferences.getBool("mqttEn", false)', CONFIG)
        self.assertIn("c6LocalMqttLoop();", MAIN)
        self.assertIn("c6AwsLoop();", MAIN)
        disabled = CLIENT.split("void c6LocalMqttLoop()", 1)[1].split(
            "String c6LocalMqttStatusJson", 1
        )[0]
        self.assertIn("if (!c6Config.localMqttEnabled)", disabled)
        self.assertNotIn("c6Config.motCloudEnabled", CLIENT)

    def test_tls_and_authentication_have_no_insecure_fallback(self) -> None:
        self.assertIn("WiFiClientSecure secureClient", CLIENT)
        self.assertIn("secureClient.setCACert", CLIENT)
        self.assertIn("secureClient.setCertificate", CLIENT)
        self.assertIn("secureClient.setPrivateKey", CLIENT)
        self.assertIn("UTC invalid; TLS connection refused", CLIENT)
        self.assertNotIn("setInsecure", CLIENT)
        self.assertNotIn("WiFiClient ", CLIENT)
        self.assertIn("!c6Config.localMqttUsername.isEmpty() && !password.isEmpty()", CLIENT)

    def test_credentials_are_separate_and_redacted(self) -> None:
        for path in (
            "/mqtt/root-ca.pem",
            "/mqtt/password.txt",
            "/mqtt/client-certificate.pem",
            "/mqtt/client-private-key.pem",
        ):
            self.assertIn(path, CLIENT)
        export = CONFIG.split("String c6ConfigExportJson", 1)[1].split(
            "bool c6ConfigImportJson", 1
        )[0]
        for secret in ("localMqttPassword", "localMqttRootCa", "localMqttClientPrivateKey"):
            self.assertNotIn(secret, export)
        status = CLIENT.split("String c6LocalMqttStatusJson", 1)[1]
        self.assertNotIn('password +', status)
        self.assertNotIn('rootCa +', status)
        self.assertNotIn('clientPrivateKey +', status)

    def test_publish_only_namespace_and_no_subscription(self) -> None:
        self.assertIn('c6Config.localMqttTopicPrefix + "/"', CLIENT)
        self.assertIn('c6Config.localMqttVehicleId + "/" + suffix', CLIENT)
        self.assertNotIn("mqttClient.subscribe", CLIENT)
        self.assertIn('topic("status/online")', CLIENT)
        self.assertIn('publish("status/online", "true")', CLIENT)

    def test_location_requires_separate_local_authenticated_action(self) -> None:
        self.assertIn("bool localMqttLocationEnabled = false", CONFIG_H)
        self.assertIn('server.on("/api/local-mqtt/location", HTTP_POST', WEB)
        handler = WEB.split("void localMqttLocation()", 1)[1].split("\n}", 1)[0]
        self.assertIn("requireAdmin()", handler)
        self.assertIn("requireSameOrigin", handler)
        self.assertIn("c6ConfigSetLocalMqttLocationEnabled", handler)
        save = WEB.split("void saveConfig()", 1)[1].split("void exportConfig()", 1)[0]
        self.assertNotIn("localMqttLocationEnabled =", save)

    def test_location_opt_out_has_durable_retained_cleanup(self) -> None:
        self.assertIn("localMqttLocationCleanupPending = true", CONFIG)
        self.assertIn('preferences.putBool("mqttLocClr"', CONFIG)
        for suffix in (
            "location/latitude",
            "location/longitude",
            "location/speed_kmph",
            "location/satellites",
            "location/hdop",
            "location/age_ms",
        ):
            self.assertIn(suffix, CLIENT)
        self.assertIn('complete &= publish(suffix, "")', CLIENT)
        self.assertIn("c6ConfigMarkLocalMqttLocationCleanupComplete", CLIENT)

    def test_backup_restore_never_grants_location_consent(self) -> None:
        restore = CONFIG.split("bool c6ConfigImportJson", 1)[1]
        self.assertIn("candidate.localMqttEnabled = false", restore)
        self.assertIn("candidate.localMqttLocationEnabled = false", restore)

    def test_credential_mutations_are_authenticated_and_same_origin(self) -> None:
        for handler_name in ("localMqttClear", "localMqttLocation"):
            handler = WEB.split(f"void {handler_name}()", 1)[1].split("\n}", 1)[0]
            self.assertIn("requireAdmin()", handler)
            self.assertIn("requireSameOrigin", handler)
        self.assertIn("blank secret fields retain stored values", WEB)
        self.assertIn("return confirm(", WEB)


if __name__ == "__main__":
    unittest.main()
