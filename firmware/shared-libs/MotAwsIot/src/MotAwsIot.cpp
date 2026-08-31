#include "MotAwsIot.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <time.h>

static String certificateSha256(const String& pem) {
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt_init(&certificate);
    const int parsed = mbedtls_x509_crt_parse(
        &certificate,
        reinterpret_cast<const unsigned char*>(pem.c_str()),
        pem.length() + 1);
    if (parsed != 0 || certificate.raw.p == nullptr) {
        mbedtls_x509_crt_free(&certificate);
        return "parse-failed";
    }

    unsigned char digest[32] = {};
    const int hashed = mbedtls_sha256(
        certificate.raw.p,
        certificate.raw.len,
        digest,
        0);
    mbedtls_x509_crt_free(&certificate);
    if (hashed != 0) return "hash-failed";

    char hex[65] = {};
    for (size_t i = 0; i < sizeof(digest); ++i) {
        snprintf(hex + (i * 2), 3, "%02x", digest[i]);
    }
    return String(hex);
}

static bool readRequiredFile(
    const String& path,
    String& value,
    String& message
) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        message = "AWS credential file missing: " + path;
        return false;
    }

    value = file.readString();
    file.close();
    value.trim();

    if (value.isEmpty()) {
        message = "AWS credential file empty: " + path;
        return false;
    }

    return true;
}

static bool littleFsPartitionIsErased() {
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
        nullptr);
    if (partition == nullptr) return false;

    uint8_t buffer[256];
    const size_t inspectionBytes = min(static_cast<size_t>(partition->size), static_cast<size_t>(8192));
    for (size_t offset = 0; offset < inspectionBytes; offset += sizeof(buffer)) {
        const size_t length = min(sizeof(buffer), inspectionBytes - offset);
        if (esp_partition_read(partition, offset, buffer, length) != ESP_OK) return false;
        for (size_t i = 0; i < length; ++i) {
            if (buffer[i] != 0xff) return false;
        }
    }
    return true;
}

static bool mountLittleFsSafely() {
    if (littleFsPartitionIsErased()) {
        Serial.println("LittleFS: blank partition after factory erase; formatting once");
        return LittleFS.begin(true);
    }
    return LittleFS.begin(false);
}

bool motLoadAwsCredentials(
    MotAwsCredentials& credentials,
    const char* basePath
) {
    credentials = MotAwsCredentials();

    if (!mountLittleFsSafely()) {
        credentials.message = "LittleFS mount failed";
        return false;
    }

    String base = basePath ? String(basePath) : String("/aws");
    while (base.endsWith("/")) base.remove(base.length() - 1);

    String deviceJson;
    if (!readRequiredFile(
            base + "/device.json",
            deviceJson,
            credentials.message)) return false;
    if (!readRequiredFile(
            base + "/AmazonRootCA1.pem",
            credentials.rootCa,
            credentials.message)) return false;
    if (!readRequiredFile(
            base + "/device-certificate.pem.crt",
            credentials.certificate,
            credentials.message)) return false;
    if (!readRequiredFile(
            base + "/device-private-key.pem.key",
            credentials.privateKey,
            credentials.message)) return false;

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, deviceJson);
    if (error) {
        credentials.message =
            String("AWS device.json invalid: ") + error.c_str();
        return false;
    }

    credentials.endpoint = doc["endpoint"] | "";
    credentials.port = doc["port"] | 8883;
    credentials.thingName = doc["thingName"] | "";
    credentials.vehicleId = doc["vehicleId"] | "";
    credentials.topicPrefix = doc["topicPrefix"] | "mot";

    credentials.endpoint.trim();
    credentials.thingName.trim();
    credentials.vehicleId.trim();
    credentials.topicPrefix.trim();

    if (credentials.endpoint.isEmpty() ||
        credentials.thingName.isEmpty() ||
        credentials.vehicleId.isEmpty()) {
        credentials.message =
            "AWS device.json missing endpoint, thingName or vehicleId";
        return false;
    }

    credentials.loaded = true;
    credentials.message = "AWS credentials loaded from LittleFS";
    return true;
}

MotAwsIotClient::MotAwsIotClient() : mqtt_(secureClient_) {}

bool MotAwsIotClient::begin(const MotAwsCredentials& credentials) {
    Serial.printf(
        "AWS IoT certificate SHA-256: %s\n",
        certificateSha256(credentials.certificate).c_str());
    secureClient_.setCACert(credentials.rootCa.c_str());
    secureClient_.setCertificate(credentials.certificate.c_str());
    secureClient_.setPrivateKey(credentials.privateKey.c_str());
    // A weak but still associated WiFi link must not stall the firmware's
    // cooperative network/CAN/GPS loop for tens of seconds.
    secureClient_.setHandshakeTimeout(7);
    secureClient_.setTimeout(5000);
    return configure(credentials, secureClient_);
}

bool MotAwsIotClient::begin(
    const MotAwsCredentials& credentials,
    Client& transportClient
) {
    return configure(credentials, transportClient);
}

bool MotAwsIotClient::configure(
    const MotAwsCredentials& credentials,
    Client& transportClient
) {
    disconnect();
    credentials_ = credentials;
    status_ = MotAwsStatus();
    status_.credentialsLoaded = credentials_.loaded;
    status_.message = credentials_.message;

    if (!credentials_.loaded) return false;

    mqtt_.setClient(transportClient);
    mqtt_.setServer(
        credentials_.endpoint.c_str(),
        credentials_.port
    );
    mqtt_.setBufferSize(1024);
    mqtt_.setKeepAlive(45);
    mqtt_.setSocketTimeout(5);

    status_.message = "AWS IoT configured";
    return true;
}

bool MotAwsIotClient::enabled() const {
    return credentials_.loaded;
}

bool MotAwsIotClient::connected() {
    return mqtt_.connected();
}

bool MotAwsIotClient::timeValid() const {
    return time(nullptr) >= MIN_VALID_UTC;
}

String MotAwsIotClient::topic(const char* suffix) const {
    String prefix = credentials_.topicPrefix;
    String vehicle = credentials_.vehicleId;

    prefix.trim();
    while (prefix.endsWith("/")) {
        prefix.remove(prefix.length() - 1);
    }
    if (prefix.isEmpty()) prefix = "mot";

    vehicle.trim();
    vehicle.replace("/", "-");
    if (vehicle.isEmpty()) vehicle = "vehicle";

    return prefix + "/" + vehicle + "/" + suffix;
}

bool MotAwsIotClient::connect() {
    if (!enabled() || mqtt_.connected() || !timeValid()) return false;

    status_.connectAttempts++;
    status_.message = "AWS IoT connecting";

    const String willTopic = topic("status/online");

    Serial.printf(
        "AWS IoT: connecting endpoint=%s port=%u clientId=%s freeHeap=%u\n",
        credentials_.endpoint.c_str(),
        credentials_.port,
        credentials_.thingName.c_str(),
        ESP.getFreeHeap()
    );

    const uint32_t connectStartedMs = millis();
    const bool ok = mqtt_.connect(
        credentials_.thingName.c_str(),
        willTopic.c_str(),
        1,
        true,
        "false"
    );

    status_.lastConnectDurationMs = millis() - connectStartedMs;
    status_.mqttState = mqtt_.state();
    status_.connected = ok;

    if (!ok) {
        char tlsError[128] = {};
        const int tlsErrorCode = secureClient_.lastError(
            tlsError,
            sizeof(tlsError));
        status_.consecutiveConnectFailures++;
        status_.totalConnectFailures++;
        reconnectDelayMs_ = min(MAX_RECONNECT_INTERVAL_MS,
                                reconnectDelayMs_ > MAX_RECONNECT_INTERVAL_MS / 2
                                    ? MAX_RECONNECT_INTERVAL_MS
                                    : reconnectDelayMs_ * 2);
        status_.reconnectDelayMs = reconnectDelayMs_;
        status_.tlsErrorCode = tlsErrorCode;
        status_.tlsError = tlsError[0] ? tlsError : "none";
        status_.message =
            "AWS IoT connect failed rc=" + String(mqtt_.state());
        Serial.println(status_.message);
        Serial.printf(
            "AWS IoT TLS diagnostic: code=%d detail=%s\n",
            tlsErrorCode,
            tlsError[0] ? tlsError : "none");
        return false;
    }

    status_.reconnectCount++;
    status_.consecutiveConnectFailures = 0;
    reconnectDelayMs_ = RECONNECT_INTERVAL_MS;
    status_.reconnectDelayMs = reconnectDelayMs_;
    status_.tlsErrorCode = 0;
    status_.tlsError = "";
    status_.message = "AWS IoT connected";
    Serial.printf(
        "AWS IoT: connected freeHeap=%u will=%s\n",
        ESP.getFreeHeap(),
        willTopic.c_str()
    );

    publishBirth();
    return true;
}

void MotAwsIotClient::loop(
    const MotAwsRuntime& runtime,
    bool networkOnline
) {
    runtime_ = runtime;
    status_.timeValid = timeValid();

    if (!enabled()) return;

    if (!networkOnline) {
        if (mqtt_.connected()) mqtt_.disconnect();
        status_.connected = false;
        status_.message = "AWS IoT waiting for network";
        previousConnected_ = false;
        lastReconnectAttemptMs_ = 0;
        reconnectDelayMs_ = RECONNECT_INTERVAL_MS;
        status_.reconnectDelayMs = reconnectDelayMs_;
        return;
    }

    if (!mqtt_.connected()) {
        const uint32_t now = millis();
        if (!lastReconnectAttemptMs_ ||
            now - lastReconnectAttemptMs_ >= reconnectDelayMs_) {
            lastReconnectAttemptMs_ = now;
            connect();
        }
    }

    const bool loopOk = mqtt_.loop();
    const bool connectedNow = mqtt_.connected();

    if (previousConnected_ && !connectedNow) {
        Serial.printf(
            "AWS IoT: connection lost state=%d loopOk=%s freeHeap=%u\n",
            mqtt_.state(),
            loopOk ? "true" : "false",
            ESP.getFreeHeap()
        );
    }

    previousConnected_ = connectedNow;
    status_.connected = connectedNow;
    status_.mqttState = mqtt_.state();

    if (connectedNow) publishHeartbeat(false);
}

void MotAwsIotClient::disconnect() {
    if (mqtt_.connected()) mqtt_.disconnect();
    status_.connected = false;
}

bool MotAwsIotClient::publish(
    const char* suffix,
    const String& payload,
    bool retained
) {
    if (!mqtt_.connected()) return false;

    const bool ok = mqtt_.publish(
        topic(suffix).c_str(),
        payload.c_str(),
        retained
    );
    if (ok) status_.publishCount++;
    return ok;
}

bool MotAwsIotClient::publishInt(
    const char* suffix,
    long value,
    bool retained
) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%ld", value);
    return publish(suffix, buffer, retained);
}

bool MotAwsIotClient::publishFloat(
    const char* suffix,
    float value,
    int decimals,
    bool retained
) {
    if (isnan(value)) return false;
    char buffer[40];
    dtostrf(value, 0, decimals, buffer);
    return publish(suffix, buffer, retained);
}

bool MotAwsIotClient::publishBool(
    const char* suffix,
    bool value,
    bool retained
) {
    return publish(suffix, value ? "true" : "false", retained);
}

bool MotAwsIotClient::publishLastSeenUtc() {
    if (!timeValid()) return false;

    char value[24];
    snprintf(
        value,
        sizeof(value),
        "%lld",
        static_cast<long long>(time(nullptr))
    );
    return publish("system/last_seen_utc", value, true);
}

void MotAwsIotClient::setMessageCallback(MotAwsMessageCallback callback) {
    mqtt_.setCallback(callback);
}

bool MotAwsIotClient::subscribe(const char* suffix, uint8_t qos) {
    if (!mqtt_.connected() || suffix == nullptr || *suffix == '\0') return false;
    return mqtt_.subscribe(topic(suffix).c_str(), qos);
}

const char* MotAwsIotClient::resetReasonText() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        default: return "unknown";
    }
}

void MotAwsIotClient::publishBirth() {
    publish("status/online", "true", true);
    publish("system/device_id", runtime_.deviceId, true);
    publish("system/device_name", runtime_.deviceName, true);
    publish("system/mqtt_client_id", credentials_.thingName, true);
    publish("system/firmware_version", runtime_.firmwareVersion, true);
    publish("system/network_mode", runtime_.networkMode, true);
    publish("system/mqtt_transport", runtime_.transport, true);
    publish("system/ip_address", runtime_.ipAddress, true);
    publish("system/boot_reason", resetReasonText(), true);
    publishLastSeenUtc();
    publishHeartbeat(true);
    status_.birthCount++;
}

void MotAwsIotClient::publishHeartbeat(bool force) {
    if (!mqtt_.connected() || !timeValid()) return;

    const uint32_t now = millis();
    if (!force && lastHeartbeatMs_ &&
        now - lastHeartbeatMs_ < HEARTBEAT_INTERVAL_MS) return;

    lastHeartbeatMs_ = now;

    char payload[360];
    snprintf(
        payload,
        sizeof(payload),
        "{\"utc\":%lld,\"uptime_sec\":%lu,\"free_heap\":%u,"
        "\"network_mode\":\"%s\",\"transport\":\"%s\","
        "\"ip_address\":\"%s\",\"wifi_rssi\":%d}",
        static_cast<long long>(time(nullptr)),
        static_cast<unsigned long>(runtime_.uptimeSec),
        ESP.getFreeHeap(),
        runtime_.networkMode.c_str(),
        runtime_.transport.c_str(),
        runtime_.ipAddress.c_str(),
        runtime_.wifiRssi
    );

    if (publish("system/heartbeat", payload, false)) {
        status_.heartbeatCount++;
    }
}

const MotAwsCredentials& MotAwsIotClient::credentials() const {
    return credentials_;
}

const MotAwsStatus& MotAwsIotClient::status() const {
    return status_;
}
