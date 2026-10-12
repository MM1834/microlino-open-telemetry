from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
C6 = ROOT / "firmware/esp32-c6/src"
CLIENT = (C6 / "c6_local_mqtt.cpp").read_text(encoding="utf-8")
CONFIG = (C6 / "c6_config.cpp").read_text(encoding="utf-8")
CONFIG_H = (C6 / "c6_config.h").read_text(encoding="utf-8")
WEB = (C6 / "c6_web.cpp").read_text(encoding="utf-8")
MAIN = (C6 / "main.cpp").read_text(encoding="utf-8")
CLOUD_WORKER = (C6 / "c6_cloud_worker.cpp").read_text(encoding="utf-8")
AWS = (C6 / "c6_aws.cpp").read_text(encoding="utf-8")
AWS_CLIENT = (ROOT / "firmware/shared-libs/MotAwsIot/src/MotAwsIot.cpp").read_text(
    encoding="utf-8"
)


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
        self.assertIn("c6Config.motCloudEnabled && c6Config.abrpEnabled", CLIENT)

    def test_display_full_cloud_mode_uses_exclusive_local_mqtt_tls_windows(self) -> None:
        self.assertIn("c6LocalMqttNeedsExclusiveTlsWindow()", CLOUD_WORKER)
        self.assertIn("c6AwsDisconnect();", CLOUD_WORKER)
        self.assertIn("c6LocalMqttUsesExclusiveTlsWindows()", CLOUD_WORKER)
        self.assertIn("c6LocalMqttExclusiveTlsCooldownActive()", CLOUD_WORKER)
        self.assertIn("EXCLUSIVE_TLS_HANDOFF_MS = 1000", CLIENT)
        self.assertIn('message = "burst publish complete; TLS released"', CLIENT)
        self.assertIn('publish("status/online", "false")', CLIENT)
        self.assertIn("secureClient.stop();", CLIENT)
        self.assertIn("c6AwsReadyForTlsPause()", CLIENT)
        self.assertIn("AWS_TLS_PAUSE_STABLE_MS = 5000", AWS)
        self.assertIn("readyForTlsPauseSnapshot", AWS)
        ready = AWS.split("bool c6AwsReadyForTlsPause()", 1)[1].split("\n}", 1)[0]
        self.assertIn("#ifdef MOT_DISPLAY_BLE", ready)
        self.assertIn("return readyForTlsPauseSnapshot", ready)

    def test_controlled_aws_tls_handoff_does_not_trigger_failure_backoff(self) -> None:
        disconnect = AWS_CLIENT.split("void MotAwsIotClient::disconnect()", 1)[1].split(
            "void MotAwsIotClient::setMinimalBirth", 1
        )[0]
        self.assertIn("previousConnected_ = false", disconnect)
        self.assertIn("connectedSinceMs_ = 0", disconnect)
        self.assertIn("reconnectDelayMs_ = RECONNECT_INTERVAL_MS", disconnect)
        self.assertIn("lastReconnectAttemptMs_ = millis()", disconnect)

    def test_tls_and_authentication_have_no_insecure_fallback(self) -> None:
        self.assertIn("ResolvedNetworkClientSecure secureClient", CLIENT)
        self.assertIn("secureClient.configureServerIdentity", CLIENT)
        self.assertIn("secureClient.resolveOnLoopTask()", CLIENT)
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

    def test_periodic_telemetry_uses_only_one_versioned_envelope(self) -> None:
        publish = CLIENT.split("bool publishTelemetry()", 1)[1].split(
            "\nvoid connect()", 1
        )[0]
        self.assertIn('document["version"] = 1', publish)
        self.assertIn('document["vehicleId"] = c6Config.localMqttVehicleId', publish)
        self.assertIn('document["values"].to<JsonObject>()', publish)
        self.assertIn('publish("telemetry/state/v1", payload)', publish)
        self.assertNotIn("publishFloat(", publish)
        self.assertNotIn("publishInt(", publish)
        self.assertNotIn("publishBool(", publish)
        self.assertIn("mqttClient.setBufferSize(5120)", CLIENT)

    def test_local_mqtt_has_independent_default_interval(self) -> None:
        self.assertIn("uint32_t localMqttPublishIntervalMs = 60000", CONFIG_H)
        self.assertIn('preferences.getUInt("mqttPubMs", 60000)', CONFIG)
        self.assertIn('preferences.putUInt("mqttPubMs"', CONFIG)
        self.assertIn('name=\'mqttPubMs\'', WEB)
        self.assertIn("c6Config.localMqttPublishIntervalMs = localMqttInterval", WEB)

    def test_upgrade_removes_legacy_retained_telemetry_once(self) -> None:
        self.assertIn("localMqttLegacyCleanupPending = true", CONFIG_H)
        self.assertIn('preferences.getBool("mqttEnv1", false)', CONFIG)
        self.assertIn("clearLegacyRetainedTelemetry()", CLIENT)
        self.assertIn("c6ConfigMarkLocalMqttEnvelopeMigrationComplete", CLIENT)

    def test_runtime_diagnostics_count_mqtt_volume_and_connection_churn(self) -> None:
        for field in (
            "connectSuccesses",
            "disconnects",
            "publishCount",
            "payloadBytes",
            "mqttPublishBytes",
        ):
            self.assertIn(field, CLIENT)
        self.assertIn("remainingLengthBytes", CLIENT)
        self.assertIn("previousConnected", CLIENT)

    def test_data_analysis_build_checkpoints_volume_counters_every_twenty_minutes(self) -> None:
        self.assertIn("#ifdef MOT_DATA_ANALYSIS_BUILD", CLIENT)
        self.assertIn("20UL * 60UL * 1000UL", CLIENT)
        self.assertIn('COUNTER_PREFERENCES_NAMESPACE[] = "mot-mqttstat"', CLIENT)
        self.assertIn("preferences.putBytes(COUNTER_SNAPSHOT_KEY", CLIENT)
        self.assertIn("loadCounterSnapshot();", CLIENT)
        self.assertIn("checkpointCountersIfDue();", CLIENT)
        self.assertIn('counterPersistence\\\":\\\"nvs-20min', CLIENT)
        self.assertIn("if (!counterSnapshotDirty", CLIENT)

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
