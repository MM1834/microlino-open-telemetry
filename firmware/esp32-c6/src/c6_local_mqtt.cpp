#include "c6_local_mqtt.h"

#include <LittleFS.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include <math.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <time.h>

#include "c6_config.h"
#include "c6_gps.h"
#include "c6_network.h"
#include "system/device_id.h"
#include "system/version.h"
#include "telemetry/telemetry.h"

namespace {
constexpr char CREDENTIAL_DIRECTORY[] = "/mqtt";
constexpr char ROOT_CA_PATH[] = "/mqtt/root-ca.pem";
constexpr char PASSWORD_PATH[] = "/mqtt/password.txt";
constexpr char CLIENT_CERTIFICATE_PATH[] = "/mqtt/client-certificate.pem";
constexpr char CLIENT_PRIVATE_KEY_PATH[] = "/mqtt/client-private-key.pem";
constexpr time_t MIN_VALID_UTC = 1704067200;  // 2024-01-01 UTC
constexpr uint32_t MIN_RECONNECT_MS = 5000;
constexpr uint32_t MAX_RECONNECT_MS = 300000;

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);
String rootCa;
String password;
String clientCertificate;
String clientPrivateKey;
String message = "not configured";
String tlsError;
uint32_t lastPublishMs = 0;
uint32_t nextConnectMs = 0;
uint32_t reconnectDelayMs = MIN_RECONNECT_MS;
uint32_t connectAttempts = 0;
uint32_t publishCount = 0;
int mqttState = MQTT_DISCONNECTED;

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
    const bool ok = mqttClient.publish(topic(suffix).c_str(), payload.c_str(), retained);
    mqttState = mqttClient.state();
    if (ok) publishCount++;
    return ok;
}

bool publishFloat(const char *suffix, float value, uint8_t decimals)
{
    return isfinite(value) && publish(suffix, String(value, static_cast<unsigned int>(decimals)));
}

bool publishInt(const char *suffix, long value)
{
    return publish(suffix, String(value));
}

bool publishBool(const char *suffix, bool value)
{
    return publish(suffix, value ? "true" : "false");
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

bool clearRetainedLocation()
{
    static const char *suffixes[] = {
        "location/latitude", "location/longitude", "location/speed_kmph",
        "location/altitude_m",
        "location/satellites", "location/hdop", "location/age_ms"
    };
    bool complete = true;
    for (const char *suffix : suffixes) complete &= publish(suffix, "");
    if (complete) c6ConfigMarkLocalMqttLocationCleanupComplete();
    return complete;
}

void publishTelemetry()
{
    if (telemetry.display.valid) {
        publishFloat("display/soc", telemetry.display.soc, 1);
        publishFloat("display/speed_kmh", telemetry.display.speedKmh, 1);
        publishFloat("display/odometer_km", telemetry.display.odometerKm, 1);
        publishInt("display/estimated_range_km", telemetry.display.estimatedRangeKm);
    }
    const bool freshBmsCurrent = telemetry.bms.packCurrentValid &&
        millis() - telemetry.bms.packCurrentLastUpdateMs <= 10000;
    const bool freshBmsStatus = telemetry.bms.statusLastUpdateMs != 0 &&
        telemetry.bms.packStatusValid &&
        millis() - telemetry.bms.statusLastUpdateMs <= 10000;
    if (freshBmsCurrent && freshBmsStatus) {
        publishBool("charging/is_charging", telemetryIsCharging());
        publishBool("charging/plugged", telemetry.bms.plugged);
        publishInt("charging/power_signed",
                   static_cast<int>(lroundf(telemetry.bms.vehiclePowerW / 100.0f)));
    } else if (telemetry.charging.valid) {
        publishBool("charging/is_charging", telemetryIsCharging());
        publishBool("charging/plugged", telemetry.charging.plugged);
        publishInt("charging/power_display", telemetry.charging.powerDisplay);
        publishInt("charging/power_signed", telemetry.charging.powerSigned);
    }
    if (telemetry.bms.packStatusValid) {
        publishFloat("bms/pack_voltage", telemetry.bms.packVoltageMv / 1000.0f, 3);
        publishInt("bms/status_byte", telemetry.bms.statusByte);
    }
    if (telemetry.bms.packCurrentValid) {
        publishFloat("bms/pack_current", telemetry.bms.packCurrentA, 1);
        publishFloat("bms/pack_power_w", telemetry.bms.packPowerW, 0);
        publishFloat("bms/vehicle_power_w", telemetry.bms.vehiclePowerW, 0);
        publishBool("bms/is_regenerating", telemetry.bms.isRegenerating);
        publishBool("bms/is_discharging", telemetry.bms.isDischarging);
    }
    if (c6Config.localMqttLocationEnabled && c6GpsValid()) {
        publishFloat("location/latitude", static_cast<float>(c6GpsLatitude()), 6);
        publishFloat("location/longitude", static_cast<float>(c6GpsLongitude()), 6);
        publishFloat("location/speed_kmph", static_cast<float>(c6GpsSpeedKmph()), 2);
        publishFloat("location/altitude_m", static_cast<float>(c6GpsAltitudeMeters()), 1);
        publishInt("location/satellites", c6GpsSatellites());
    }
    publishInt("system/uptime_sec", millis() / 1000UL);
    publishInt("system/wifi_rssi", c6NetworkRssi());
    publish("system/ip_address", c6NetworkIp());
    publish("system/network_mode", "WiFi STA");
    publish("system/mqtt_transport", "WiFi TLS");
    if (time(nullptr) >= MIN_VALID_UTC) publishInt("system/last_seen_utc", time(nullptr));
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
    message = "connected";
    publishBirth();
    if (c6Config.localMqttLocationCleanupPending &&
        !c6Config.localMqttLocationEnabled) clearRetainedLocation();
}
}

void c6LocalMqttSetup()
{
    mqttClient.setBufferSize(1024);
    mqttClient.setKeepAlive(45);
    mqttClient.setSocketTimeout(5);
    loadCredentials();
    if (!c6Config.localMqttEnabled) message = "disabled; credentials retained";
}

void c6LocalMqttLoop()
{
    if (!c6Config.localMqttEnabled) {
        if (mqttClient.connected()) mqttClient.disconnect();
        return;
    }
    if (!c6NetworkTransportReady()) return;
    if (!mqttClient.connected()) {
        if (static_cast<int32_t>(millis() - nextConnectMs) >= 0) connect();
        return;
    }
    mqttClient.loop();
    if (c6Config.localMqttLocationCleanupPending &&
        !c6Config.localMqttLocationEnabled) clearRetainedLocation();
    if (millis() - lastPublishMs >= c6Config.publishIntervalMs) {
        lastPublishMs = millis();
        publishTelemetry();
    }
}

String c6LocalMqttStatusJson()
{
    return "{\"enabled\":" + String(c6Config.localMqttEnabled ? "true" : "false") +
        ",\"connected\":" + String(mqttClient.connected() ? "true" : "false") +
        ",\"credentialsReady\":" + String(credentialsReady() ? "true" : "false") +
        ",\"authentication\":\"" + String(c6Config.localMqttUseClientCertificate ? "client-certificate" : "username-password") + "\"" +
        ",\"locationEnabled\":" + String(c6Config.localMqttLocationEnabled ? "true" : "false") +
        ",\"locationCleanupPending\":" + String(c6Config.localMqttLocationCleanupPending ? "true" : "false") +
        ",\"mqttState\":" + String(mqttState) +
        ",\"connectAttempts\":" + String(connectAttempts) +
        ",\"publishCount\":" + String(publishCount) +
        ",\"retryMs\":" + String(reconnectDelayMs) +
        ",\"tlsError\":\"" + jsonEscape(tlsError) + "\"" +
        ",\"message\":\"" + jsonEscape(message) + "\"}";
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
