#include "c6_local_mqtt.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#ifdef MOT_DATA_ANALYSIS_BUILD
#include <Preferences.h>
#endif
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include <math.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <time.h>

#include "c6_config.h"
#include "c6_aws.h"
#include "c6_gps.h"
#include "c6_network.h"
#include "c6_telemetry_cadence.h"
#include "system/device_id.h"
#include "system/version.h"
#include "telemetry/telemetry.h"
#include "network/resolved_network_client_secure.h"

namespace {
constexpr char CREDENTIAL_DIRECTORY[] = "/mqtt";
constexpr char ROOT_CA_PATH[] = "/mqtt/root-ca.pem";
constexpr char PASSWORD_PATH[] = "/mqtt/password.txt";
constexpr char CLIENT_CERTIFICATE_PATH[] = "/mqtt/client-certificate.pem";
constexpr char CLIENT_PRIVATE_KEY_PATH[] = "/mqtt/client-private-key.pem";
constexpr time_t MIN_VALID_UTC = 1704067200;  // 2024-01-01 UTC
constexpr uint32_t MIN_RECONNECT_MS = 5000;
constexpr uint32_t MAX_RECONNECT_MS = 300000;
constexpr uint32_t EXCLUSIVE_TLS_HANDOFF_MS = 1000;
constexpr uint32_t ALTITUDE_DISPLAY_MAX_AGE_MS = 3000;
constexpr float ALTITUDE_MIN_MOVING_SPEED_KMH = 1.0f;
#ifdef MOT_DATA_ANALYSIS_BUILD
constexpr char COUNTER_PREFERENCES_NAMESPACE[] = "mot-mqttstat";
constexpr char COUNTER_SNAPSHOT_KEY[] = "counters";
constexpr uint32_t COUNTER_SNAPSHOT_SCHEMA = 1;
constexpr uint32_t COUNTER_CHECKPOINT_INTERVAL_MS = 20UL * 60UL * 1000UL;

struct CounterSnapshot {
    uint32_t schema;
    uint32_t publishCount;
    uint64_t payloadBytes;
    uint64_t mqttPublishBytes;
    uint32_t checkpoints;
};
#endif

bool altitudePublishable()
{
    const bool displayFresh = telemetry.display.valid &&
        static_cast<uint32_t>(millis() - telemetry.display.lastUpdateMs) <=
            ALTITUDE_DISPLAY_MAX_AGE_MS;
    const bool moving = displayFresh && isfinite(telemetry.display.speedKmh) &&
        telemetry.display.speedKmh > ALTITUDE_MIN_MOVING_SPEED_KMH;
    return c6GpsAltitudePublishable(moving);
}

ResolvedNetworkClientSecure secureClient;
PubSubClient mqttClient(secureClient);
String rootCa;
String password;
String clientCertificate;
String clientPrivateKey;
String message = "not configured";
String tlsError;
uint32_t lastPublishMs = 0;
uint32_t telemetrySequence = 0;
uint32_t lastCadenceEventGeneration = 0;
bool forceTelemetryPublish = true;
bool birthPublished = false;
uint32_t exclusiveTlsReleasedMs = 0;
uint32_t nextConnectMs = 0;
uint32_t reconnectDelayMs = MIN_RECONNECT_MS;
uint32_t connectAttempts = 0;
uint32_t publishCount = 0;
uint32_t connectSuccessCount = 0;
uint32_t disconnectCount = 0;
uint64_t payloadBytes = 0;
uint64_t mqttPublishBytes = 0;
#ifdef MOT_DISPLAY_BLE
portMUX_TYPE statusSnapshotMux = portMUX_INITIALIZER_UNLOCKED;
char statusSnapshot[1536] = "{\"message\":\"starting\"}";
volatile bool connectedSnapshot = false;
#endif
#ifdef MOT_DATA_ANALYSIS_BUILD
uint32_t counterCheckpoints = 0;
uint32_t counterCheckpointFailures = 0;
uint32_t lastCounterCheckpointMs = 0;
bool counterSnapshotRestored = false;
bool counterSnapshotDirty = false;
#endif
bool previousConnected = false;
volatile bool endpointPrepared = false;
String preparedNetworkIp;
int mqttState = MQTT_DISCONNECTED;

bool exclusiveTlsWindowMode()
{
#ifdef MOT_DISPLAY_BLE
    return c6Config.motCloudEnabled && c6Config.abrpEnabled;
#else
    return false;
#endif
}

bool telemetryDue()
{
    const uint32_t cadenceGeneration = c6TelemetryCadenceEventGeneration();
    const uint32_t intervalMs =
        c6TelemetryCadenceIntervalMs(c6Config.localMqttPublishIntervalMs);
    return forceTelemetryPublish || cadenceGeneration != lastCadenceEventGeneration ||
        millis() - lastPublishMs >= intervalMs;
}

#ifdef MOT_DATA_ANALYSIS_BUILD
void loadCounterSnapshot()
{
    Preferences preferences;
    if (!preferences.begin(COUNTER_PREFERENCES_NAMESPACE, true)) return;
    if (preferences.getBytesLength(COUNTER_SNAPSHOT_KEY) == sizeof(CounterSnapshot)) {
        CounterSnapshot snapshot = {};
        if (preferences.getBytes(COUNTER_SNAPSHOT_KEY, &snapshot, sizeof(snapshot)) ==
                sizeof(snapshot) &&
            snapshot.schema == COUNTER_SNAPSHOT_SCHEMA) {
            publishCount = snapshot.publishCount;
            payloadBytes = snapshot.payloadBytes;
            mqttPublishBytes = snapshot.mqttPublishBytes;
            counterCheckpoints = snapshot.checkpoints;
            counterSnapshotRestored = true;
        }
    }
    preferences.end();
    lastCounterCheckpointMs = millis();
}

void checkpointCountersIfDue()
{
    const uint32_t now = millis();
    if (!counterSnapshotDirty ||
        static_cast<uint32_t>(now - lastCounterCheckpointMs) <
            COUNTER_CHECKPOINT_INTERVAL_MS) return;

    CounterSnapshot snapshot = {
        COUNTER_SNAPSHOT_SCHEMA,
        publishCount,
        payloadBytes,
        mqttPublishBytes,
        counterCheckpoints + 1,
    };
    Preferences preferences;
    const bool opened = preferences.begin(COUNTER_PREFERENCES_NAMESPACE, false);
    const size_t written = opened
        ? preferences.putBytes(COUNTER_SNAPSHOT_KEY, &snapshot, sizeof(snapshot))
        : 0;
    if (opened) preferences.end();
    lastCounterCheckpointMs = now;
    if (written == sizeof(snapshot)) {
        counterCheckpoints = snapshot.checkpoints;
        counterSnapshotDirty = false;
    } else {
        counterCheckpointFailures++;
    }
}
#endif

String uint64Text(uint64_t value)
{
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%llu",
             static_cast<unsigned long long>(value));
    return String(buffer);
}

String jsonEscape(String value)
{
    value.replace("\\", "\\\\"); value.replace("\"", "\\\"");
    value.replace("\n", "\\n"); value.replace("\r", "\\r");
    return value;
}

bool readFile(const char *path, String &value)
{
    File file = LittleFS.open(path, "r");
    if (!file) { value = ""; return false; }
    value = file.readString();
    file.close();
    value.trim();
    return !value.isEmpty();
}

bool writeFileAtomically(const char *path, const String &value, String &error)
{
    const String temporaryPath = String(path) + ".tmp";
    File file = LittleFS.open(temporaryPath, "w");
    if (!file) { error = "credential temporary file open failed"; return false; }
    const size_t written = file.print(value);
    file.flush();
    file.close();
    if (written != value.length()) {
        LittleFS.remove(temporaryPath);
        error = "credential write incomplete";
        return false;
    }
    LittleFS.remove(path);
    if (!LittleFS.rename(temporaryPath, path)) {
        LittleFS.remove(temporaryPath);
        error = "credential activation failed";
        return false;
    }
    return true;
}

bool validCertificatePem(const String &value)
{
    return value.length() <= 12288 &&
        value.indexOf("-----BEGIN CERTIFICATE-----") >= 0 &&
        value.indexOf("-----END CERTIFICATE-----") >= 0;
}

int mqttValidationRandom(void *, unsigned char *output, size_t length)
{
    esp_fill_random(output, length);
    return 0;
}

bool parsesCertificate(const String &value)
{
    if (!validCertificatePem(value)) return false;
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt_init(&certificate);
    const int result = mbedtls_x509_crt_parse(&certificate,
        reinterpret_cast<const unsigned char *>(value.c_str()), value.length() + 1);
    mbedtls_x509_crt_free(&certificate);
    return result == 0;
}

bool validPrivateKeyPem(const String &value)
{
    return value.length() <= 8192 &&
        ((value.indexOf("-----BEGIN PRIVATE KEY-----") >= 0 &&
          value.indexOf("-----END PRIVATE KEY-----") >= 0) ||
         (value.indexOf("-----BEGIN RSA PRIVATE KEY-----") >= 0 &&
          value.indexOf("-----END RSA PRIVATE KEY-----") >= 0) ||
         (value.indexOf("-----BEGIN EC PRIVATE KEY-----") >= 0 &&
          value.indexOf("-----END EC PRIVATE KEY-----") >= 0));
}

bool certificateMatchesPrivateKey(const String &certificateValue, const String &keyValue)
{
    if (!validCertificatePem(certificateValue) || !validPrivateKeyPem(keyValue)) return false;
    mbedtls_x509_crt certificate;
    mbedtls_pk_context key;
    mbedtls_x509_crt_init(&certificate);
    mbedtls_pk_init(&key);
    const int certificateResult = mbedtls_x509_crt_parse(&certificate,
        reinterpret_cast<const unsigned char *>(certificateValue.c_str()),
        certificateValue.length() + 1);
    const int keyResult = mbedtls_pk_parse_key(&key,
        reinterpret_cast<const unsigned char *>(keyValue.c_str()), keyValue.length() + 1,
        nullptr, 0, mqttValidationRandom, nullptr);
    const bool matches = certificateResult == 0 && keyResult == 0 &&
        mbedtls_pk_check_pair(&certificate.pk, &key, mqttValidationRandom, nullptr) == 0;
    mbedtls_pk_free(&key);
    mbedtls_x509_crt_free(&certificate);
    return matches;
}

String topic(const char *suffix)
{
    return c6Config.localMqttTopicPrefix + "/" +
        c6Config.localMqttVehicleId + "/" + suffix;
}

bool publish(const char *suffix, const String &payload, bool retained = true)
{
    if (!mqttClient.connected()) return false;
    const String fullTopic = topic(suffix);
    const bool ok = mqttClient.publish(fullTopic.c_str(), payload.c_str(), retained);
    mqttState = mqttClient.state();
    if (ok) {
        publishCount++;
        payloadBytes += payload.length();
        const size_t remainingLength = 2 + fullTopic.length() + payload.length();
        size_t remainingLengthBytes = 1;
        for (size_t value = remainingLength; value >= 128; value /= 128) {
            remainingLengthBytes++;
        }
        mqttPublishBytes += 1 + remainingLengthBytes + remainingLength;
#ifdef MOT_DATA_ANALYSIS_BUILD
        counterSnapshotDirty = true;
#endif
    }
    return ok;
}

bool credentialsReady()
{
    if (!parsesCertificate(rootCa)) return false;
    if (c6Config.localMqttUseClientCertificate) {
        return certificateMatchesPrivateKey(clientCertificate, clientPrivateKey);
    }
    return !c6Config.localMqttUsername.isEmpty() && !password.isEmpty();
}

bool configurationReady()
{
    return !c6Config.localMqttHost.isEmpty() &&
        c6Config.localMqttPort != 0 &&
        !c6Config.localMqttVehicleId.isEmpty() &&
        !c6Config.localMqttTopicPrefix.isEmpty() && credentialsReady();
}

void loadCredentials()
{
    if (!LittleFS.begin(false)) {
        message = "LittleFS unavailable; refusing MQTT credentials";
        return;
    }
    readFile(ROOT_CA_PATH, rootCa);
    readFile(PASSWORD_PATH, password);
    readFile(CLIENT_CERTIFICATE_PATH, clientCertificate);
    readFile(CLIENT_PRIVATE_KEY_PATH, clientPrivateKey);
    message = credentialsReady() ? "credentials loaded" : "credentials incomplete";
}

void scheduleReconnect()
{
    const uint32_t jitter = esp_random() % 1000;
    nextConnectMs = millis() + reconnectDelayMs + jitter;
    reconnectDelayMs = min(reconnectDelayMs * 2, MAX_RECONNECT_MS);
}

void publishBirth()
{
    publish("status/online", "true");
    publish("system/device_id", motDeviceId());
    publish("system/device_name", motHostname());
    publish("system/mqtt_client_id", motHostname() + "-local-mqtt");
    publish("system/board", MOT_BOARD);
    publish("system/firmware_version", MOT_VERSION);
    publish("system/can1_profile", decoderProfileKey(c6Config.can1Profile));
    publish("system/can2_profile", decoderProfileKey(c6Config.can2Profile));
}

bool clearLegacyRetainedTelemetry()
{
    static const char *suffixes[] = {
        "display/soc", "display/speed_kmh", "display/odometer_km",
        "display/estimated_range_km", "access/central_lock_valid",
        "access/central_locked", "access/door_open", "charging/is_charging",
        "charging/plugged", "charging/power_display", "charging/power_signed",
        "bms/pack_voltage", "bms/status_byte", "bms/soc_internal",
        "bms/soc_display", "bms/standard_soc", "bms/soh_percent",
        "bms/pack_current", "bms/pack_power_w", "bms/vehicle_power_w",
        "bms/is_regenerating", "bms/is_discharging", "bms/cell_min_mv",
        "bms/cell_max_mv", "bms/cell_delta_mv", "location/latitude",
        "location/longitude", "location/speed_kmph", "location/altitude_m",
        "location/satellites", "location/hdop", "location/age_ms",
        "system/uptime_sec", "system/wifi_rssi", "system/last_seen_utc"
    };
    bool complete = true;
    for (const char *suffix : suffixes) complete &= publish(suffix, "");
    if (complete) c6ConfigMarkLocalMqttEnvelopeMigrationComplete();
    return complete;
}

bool clearRetainedLocation()
{
    static const char *suffixes[] = {
        "location/latitude", "location/longitude", "location/speed_kmph",
        "location/altitude_m",
        "location/satellites", "location/hdop", "location/age_ms"
    };
    bool complete = true;
    for (const char *suffix : suffixes) complete &= publish(suffix, "");
    if (complete) {
        c6ConfigMarkLocalMqttLocationCleanupComplete();
        forceTelemetryPublish = true;
    }
    return complete;
}

bool publishTelemetry()
{
    if (!mqttClient.connected()) return false;
    JsonDocument document;
    document["version"] = 1;
    document["vehicleId"] = c6Config.localMqttVehicleId;
    document["deviceId"] = motDeviceId();
    document["sequence"] = ++telemetrySequence;
    document["sampledAt"] = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
    JsonObject values = document["values"].to<JsonObject>();
    if (telemetry.display.valid) {
        values["display/soc"] = roundf(telemetry.display.soc * 10.0f) / 10.0f;
        values["display/speed_kmh"] = roundf(telemetry.display.speedKmh * 10.0f) / 10.0f;
        values["display/odometer_km"] = roundf(telemetry.display.odometerKm * 10.0f) / 10.0f;
        values["display/estimated_range_km"] = telemetry.display.estimatedRangeKm;
    }
    values["access/central_lock_valid"] = telemetry.access.centralLockValid;
    if (telemetry.access.centralLockValid)
        values["access/central_locked"] = telemetry.access.centralLocked;
    if (telemetry.access.doorOpenValid)
        values["access/door_open"] = telemetry.access.doorOpen;
    const bool freshBmsCurrent = telemetry.bms.packCurrentValid &&
        millis() - telemetry.bms.packCurrentLastUpdateMs <= 10000;
    const bool freshBmsStatus = telemetry.bms.statusLastUpdateMs != 0 &&
        telemetry.bms.packStatusValid &&
        millis() - telemetry.bms.statusLastUpdateMs <= 10000;
    if (freshBmsCurrent && freshBmsStatus) {
        values["charging/is_charging"] = telemetryIsCharging();
        values["charging/plugged"] = telemetry.bms.plugged;
        values["charging/power_signed"] =
            static_cast<int>(lroundf(telemetry.bms.vehiclePowerW / 100.0f));
    } else if (telemetry.charging.valid) {
        values["charging/is_charging"] = telemetryIsCharging();
        values["charging/power_display"] = telemetry.charging.powerDisplay;
        values["charging/power_signed"] = telemetry.charging.powerSigned;
    }
    if (telemetry.bms.packStatusValid) {
        values["bms/pack_voltage"] = roundf(telemetry.bms.packVoltageMv) / 1000.0f;
        values["bms/status_byte"] = telemetry.bms.statusByte;
    }
    if (telemetry.bms.pioneerSocValid) {
        values["bms/soc_internal"] = roundf(telemetry.bms.socInternal * 10.0f) / 10.0f;
        values["bms/soc_display"] = roundf(telemetry.bms.socDisplay * 10.0f) / 10.0f;
    }
    if (telemetry.bms.standardSocValid)
        values["bms/standard_soc"] = roundf(telemetry.bms.socPercent * 100.0f) / 100.0f;
    if (telemetry.bms.sohValid)
        values["bms/soh_percent"] = roundf(telemetry.bms.sohPercent * 100.0f) / 100.0f;
    if (telemetry.bms.packCurrentValid) {
        values["bms/pack_current"] = roundf(telemetry.bms.packCurrentA * 10.0f) / 10.0f;
        values["bms/pack_power_w"] = lroundf(telemetry.bms.packPowerW);
        values["bms/vehicle_power_w"] = lroundf(telemetry.bms.vehiclePowerW);
        values["bms/is_regenerating"] = telemetry.bms.isRegenerating;
        values["bms/is_discharging"] = telemetry.bms.isDischarging;
    }
    if (telemetry.bms.cellVoltagesValid) {
        values["bms/cell_min_mv"] = telemetry.bms.minCellVoltageMv;
        values["bms/cell_max_mv"] = telemetry.bms.maxCellVoltageMv;
        values["bms/cell_delta_mv"] = telemetry.bms.cellVoltageDeltaMv;
    }
    if (c6Config.localMqttLocationEnabled && c6GpsValid()) {
        values["location/latitude"] = round(c6GpsLatitude() * 1000000.0) / 1000000.0;
        values["location/longitude"] = round(c6GpsLongitude() * 1000000.0) / 1000000.0;
        values["location/speed_kmph"] = round(c6GpsSpeedKmph() * 100.0) / 100.0;
        if (altitudePublishable())
            values["location/altitude_m"] = round(c6GpsAltitudeMeters() * 10.0) / 10.0;
        values["location/satellites"] = c6GpsSatellites();
    }
    values["system/uptime_sec"] = millis() / 1000UL;
    values["system/wifi_rssi"] = c6NetworkRssi();
    values["system/ip_address"] = c6NetworkIp();
    values["system/network_mode"] = "WiFi STA";
    values["system/mqtt_transport"] = "WiFi TLS";
    values["system/last_seen_utc"] = static_cast<uint64_t>(time(nullptr));
    String payload;
    payload.reserve(2048);
    serializeJson(document, payload);
    if (payload.length() > 4096) return false;
    return publish("telemetry/state/v1", payload);
}

void connect()
{
    if (!configurationReady()) { message = "configuration or credentials incomplete"; return; }
    if (time(nullptr) < MIN_VALID_UTC) { message = "UTC invalid; TLS connection refused"; scheduleReconnect(); return; }

    connectAttempts++;
    tlsError = "";
    secureClient.stop();
    secureClient.setCACert(rootCa.c_str());
    if (c6Config.localMqttUseClientCertificate) {
        secureClient.setCertificate(clientCertificate.c_str());
        secureClient.setPrivateKey(clientPrivateKey.c_str());
    }
    secureClient.setHandshakeTimeout(7);
    secureClient.setTimeout(5000);
    mqttClient.setServer(c6Config.localMqttHost.c_str(), c6Config.localMqttPort);

    const String clientId = motHostname() + "-local-mqtt";
    const String willTopic = topic("status/online");
    bool ok = false;
    if (c6Config.localMqttUseClientCertificate) {
        ok = mqttClient.connect(clientId.c_str(), willTopic.c_str(), 1, true, "false");
    } else {
        ok = mqttClient.connect(clientId.c_str(), c6Config.localMqttUsername.c_str(),
            password.c_str(), willTopic.c_str(), 1, true, "false");
    }
    mqttState = mqttClient.state();
    if (!ok) {
        char detail[160] = {};
        secureClient.lastError(detail, sizeof(detail));
        tlsError = detail;
        message = "connection failed";
        scheduleReconnect();
        return;
    }
    reconnectDelayMs = MIN_RECONNECT_MS;
    nextConnectMs = 0;
    connectSuccessCount++;
    previousConnected = true;
    message = "connected";
    if (!birthPublished) {
        publishBirth();
        birthPublished = true;
    }
    forceTelemetryPublish = true;
    if (c6Config.localMqttLegacyCleanupPending) clearLegacyRetainedTelemetry();
    if (c6Config.localMqttLocationCleanupPending &&
        !c6Config.localMqttLocationEnabled) clearRetainedLocation();
}

String buildLocalMqttStatusJson()
{
    return "{\"enabled\":" + String(c6Config.localMqttEnabled ? "true" : "false") +
        ",\"connected\":" + String(mqttClient.connected() ? "true" : "false") +
        ",\"credentialsReady\":" + String(credentialsReady() ? "true" : "false") +
        ",\"authentication\":\"" + String(c6Config.localMqttUseClientCertificate ? "client-certificate" : "username-password") + "\"" +
        ",\"locationEnabled\":" + String(c6Config.localMqttLocationEnabled ? "true" : "false") +
        ",\"locationCleanupPending\":" + String(c6Config.localMqttLocationCleanupPending ? "true" : "false") +
        ",\"mqttState\":" + String(mqttState) +
        ",\"connectAttempts\":" + String(connectAttempts) +
        ",\"connectSuccesses\":" + String(connectSuccessCount) +
        ",\"disconnects\":" + String(disconnectCount) +
        ",\"publishCount\":" + String(publishCount) +
        ",\"payloadBytes\":" + uint64Text(payloadBytes) +
        ",\"mqttPublishBytes\":" + uint64Text(mqttPublishBytes) +
        ",\"format\":\"telemetry-state-v1\"" +
        ",\"cadence\":\"" + c6TelemetryCadenceMode() + "\"" +
        ",\"intervalMs\":" + String(c6TelemetryCadenceIntervalMs(
            c6Config.localMqttPublishIntervalMs)) +
#ifdef MOT_DATA_ANALYSIS_BUILD
        ",\"counterPersistence\":\"nvs-20min\"" +
        ",\"counterSnapshotRestored\":" + String(counterSnapshotRestored ? "true" : "false") +
        ",\"counterCheckpoints\":" + String(counterCheckpoints) +
        ",\"counterCheckpointFailures\":" + String(counterCheckpointFailures) +
        ",\"counterCheckpointAgeSec\":" + String((millis() - lastCounterCheckpointMs) / 1000UL) +
#endif
        ",\"retryMs\":" + String(reconnectDelayMs) +
        ",\"tlsError\":\"" + jsonEscape(tlsError) + "\"" +
        ",\"message\":\"" + jsonEscape(message) + "\"}";
}

void refreshStatusSnapshot()
{
#ifdef MOT_DISPLAY_BLE
    const String value = buildLocalMqttStatusJson();
    const bool connected = mqttClient.connected();
    portENTER_CRITICAL(&statusSnapshotMux);
    strlcpy(statusSnapshot, value.c_str(), sizeof(statusSnapshot));
    connectedSnapshot = connected;
    portEXIT_CRITICAL(&statusSnapshotMux);
#endif
}
}

void c6LocalMqttSetup()
{
#ifdef MOT_DATA_ANALYSIS_BUILD
    loadCounterSnapshot();
#endif
    mqttClient.setBufferSize(5120);
    mqttClient.setKeepAlive(45);
    mqttClient.setSocketTimeout(5);
    loadCredentials();
    secureClient.configureServerIdentity(c6Config.localMqttHost, rootCa,
        c6Config.localMqttUseClientCertificate ? clientCertificate : String(),
        c6Config.localMqttUseClientCertificate ? clientPrivateKey : String());
    if (!c6Config.localMqttEnabled) message = "disabled; credentials retained";
    refreshStatusSnapshot();
}

void c6LocalMqttLoop()
{
#ifdef MOT_DATA_ANALYSIS_BUILD
    checkpointCountersIfDue();
#endif
    if (!c6Config.localMqttEnabled) {
        if (mqttClient.connected()) mqttClient.disconnect();
        previousConnected = false;
        refreshStatusSnapshot();
        return;
    }
    if (!c6NetworkTransportReady()) {
        refreshStatusSnapshot();
        return;
    }
    if (!endpointPrepared) {
        refreshStatusSnapshot();
        return;
    }
    if (!mqttClient.connected()) {
        if (previousConnected) {
            disconnectCount++;
            previousConnected = false;
        }
        if ((!exclusiveTlsWindowMode() || telemetryDue()) &&
            static_cast<int32_t>(millis() - nextConnectMs) >= 0) connect();
        refreshStatusSnapshot();
        return;
    }
    mqttClient.loop();
    if (c6Config.localMqttLegacyCleanupPending) clearLegacyRetainedTelemetry();
    if (c6Config.localMqttLocationCleanupPending &&
        !c6Config.localMqttLocationEnabled) clearRetainedLocation();
    const uint32_t cadenceGeneration = c6TelemetryCadenceEventGeneration();
    const uint32_t intervalMs =
        c6TelemetryCadenceIntervalMs(c6Config.localMqttPublishIntervalMs);
    const bool eventPending = cadenceGeneration != lastCadenceEventGeneration;
    if (forceTelemetryPublish || eventPending || millis() - lastPublishMs >= intervalMs) {
        if (publishTelemetry()) {
            lastPublishMs = millis();
            lastCadenceEventGeneration = cadenceGeneration;
            forceTelemetryPublish = false;
            if (exclusiveTlsWindowMode()) {
                publish("status/online", "false");
                mqttClient.disconnect();
                secureClient.stop();
                exclusiveTlsReleasedMs = millis();
                previousConnected = false;
                message = "burst publish complete; TLS released";
            }
        }
    }
    refreshStatusSnapshot();
}

void c6LocalMqttPrepareNetwork()
{
    if (!c6Config.localMqttEnabled || !c6NetworkTransportReady()) {
        endpointPrepared = false;
        preparedNetworkIp = "";
        return;
    }
    const String networkIp = c6NetworkIp();
    if (endpointPrepared && preparedNetworkIp == networkIp) return;
    endpointPrepared = false;
    if (secureClient.resolveOnLoopTask()) {
        preparedNetworkIp = networkIp;
        endpointPrepared = true;
        Serial.println("Local MQTT: endpoint resolved on loop task");
    }
}

void c6LocalMqttDisconnect()
{
    if (mqttClient.connected()) mqttClient.disconnect();
    secureClient.stop();
    previousConnected = false;
    refreshStatusSnapshot();
}

bool c6LocalMqttUsesExclusiveTlsWindows() { return exclusiveTlsWindowMode(); }

bool c6LocalMqttNeedsExclusiveTlsWindow()
{
    if (!exclusiveTlsWindowMode() || !c6Config.localMqttEnabled ||
        !c6NetworkTransportReady() || !endpointPrepared ||
        !c6AwsReadyForTlsPause()) return false;
    if (mqttClient.connected()) return true;
    return time(nullptr) >= MIN_VALID_UTC && telemetryDue() &&
        static_cast<int32_t>(millis() - nextConnectMs) >= 0;
}

bool c6LocalMqttExclusiveTlsCooldownActive()
{
    return exclusiveTlsWindowMode() && exclusiveTlsReleasedMs != 0 &&
        millis() - exclusiveTlsReleasedMs < EXCLUSIVE_TLS_HANDOFF_MS;
}

bool c6LocalMqttConnected()
{
#ifdef MOT_DISPLAY_BLE
    return connectedSnapshot;
#else
    return mqttClient.connected();
#endif
}

String c6LocalMqttStatusJson()
{
#ifdef MOT_DISPLAY_BLE
    char value[sizeof(statusSnapshot)] = {};
    portENTER_CRITICAL(&statusSnapshotMux);
    strlcpy(value, statusSnapshot, sizeof(value));
    portEXIT_CRITICAL(&statusSnapshotMux);
    return String(value);
#else
    return buildLocalMqttStatusJson();
#endif
}

bool c6LocalMqttCredentialsReady() { return credentialsReady(); }

bool c6LocalMqttStoreCredentials(
    const String &rootCaValue,
    const String &passwordValue,
    const String &clientCertificateValue,
    const String &clientPrivateKeyValue,
    String &error)
{
    if (!LittleFS.begin(false)) { error = "LittleFS unavailable"; return false; }
    LittleFS.mkdir(CREDENTIAL_DIRECTORY);
    String candidateRootCa = rootCaValue.isEmpty() ? rootCa : rootCaValue;
    String candidatePassword = passwordValue.isEmpty() ? password : passwordValue;
    String candidateCertificate = clientCertificateValue.isEmpty()
        ? clientCertificate : clientCertificateValue;
    String candidatePrivateKey = clientPrivateKeyValue.isEmpty()
        ? clientPrivateKey : clientPrivateKeyValue;
    if (candidateRootCa.isEmpty() && candidatePassword.isEmpty() &&
        candidateCertificate.isEmpty() && candidatePrivateKey.isEmpty()) return true;
    if (!parsesCertificate(candidateRootCa)) {
        error = "invalid or oversized root CA PEM"; return false;
    }
    if (candidatePassword.length() > 512) { error = "password too long"; return false; }
    if ((!candidateCertificate.isEmpty() || !candidatePrivateKey.isEmpty()) &&
        !certificateMatchesPrivateKey(candidateCertificate, candidatePrivateKey)) {
        error = "client certificate and private key are invalid or do not match"; return false;
    }
    if (!rootCaValue.isEmpty() && !writeFileAtomically(ROOT_CA_PATH, rootCaValue, error)) return false;
    if (!passwordValue.isEmpty() && !writeFileAtomically(PASSWORD_PATH, passwordValue, error)) return false;
    if (!clientCertificateValue.isEmpty() &&
        !writeFileAtomically(CLIENT_CERTIFICATE_PATH, clientCertificateValue, error)) return false;
    if (!clientPrivateKeyValue.isEmpty() &&
        !writeFileAtomically(CLIENT_PRIVATE_KEY_PATH, clientPrivateKeyValue, error)) return false;
    loadCredentials();
    return true;
}

void c6LocalMqttClearCredentials()
{
    c6ConfigSetLocalMqttEnabled(false);
    mqttClient.disconnect();
    secureClient.stop();
    if (LittleFS.begin(false)) {
        LittleFS.remove(ROOT_CA_PATH);
        LittleFS.remove(PASSWORD_PATH);
        LittleFS.remove(CLIENT_CERTIFICATE_PATH);
        LittleFS.remove(CLIENT_PRIVATE_KEY_PATH);
    }
    rootCa = ""; password = ""; clientCertificate = ""; clientPrivateKey = "";
    message = "disabled; credentials deleted";
}
