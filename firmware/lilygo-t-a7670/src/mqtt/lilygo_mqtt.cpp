#include "lilygo_mqtt.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

#include "config/lilygo_config.h"
#include "network/lilygo_network.h"
#include "modem/lilygo_modem.h"
#include "gps/l76k_gps.h"
#include "telemetry/telemetry.h"

static const unsigned long MQTT_PUBLISH_INTERVAL_MS = 5000;
static unsigned long lastPublishMs = 0;

static String esc(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}

static const char* mqttStateText(int state)
{
    switch (state) {
        case MQTT_CONNECTION_TIMEOUT: return "connection timeout";
        case MQTT_CONNECTION_LOST: return "connection lost";
        case MQTT_CONNECT_FAILED: return "connect failed";
        case MQTT_DISCONNECTED: return "disconnected";
        case MQTT_CONNECTED: return "connected";
        case MQTT_CONNECT_BAD_PROTOCOL: return "bad protocol";
        case MQTT_CONNECT_BAD_CLIENT_ID: return "bad client id";
        case MQTT_CONNECT_UNAVAILABLE: return "server unavailable";
        case MQTT_CONNECT_BAD_CREDENTIALS: return "bad credentials";
        case MQTT_CONNECT_UNAUTHORIZED: return "unauthorized";
        default: return "unknown";
    }
}

#ifdef MOT_AWS_IOT

#include <MotAwsIot.h>

static MotAwsCredentials awsCredentials;
static MotAwsIotClient awsClient;
static bool ntpSyncRequested = false;

static void requestUtcSyncIfPossible()
{
    if (ntpSyncRequested || WiFi.status() != WL_CONNECTED) return;

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ntpSyncRequested = true;
    Serial.println("UTC: NTP synchronization requested via WiFi");
}

static MotAwsRuntime awsRuntime()
{
    MotAwsRuntime runtime;
    runtime.deviceId = lilygoDeviceName();
    runtime.deviceName = lilygoDeviceName();
    runtime.firmwareVersion = telemetry.system.firmwareVersion;
    runtime.networkMode = lilygoNetworkModeName();
    runtime.transport = "WiFi";
    runtime.ipAddress = lilygoNetworkIp();
    runtime.wifiRssi =
        WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    runtime.uptimeSec = millis() / 1000UL;
    return runtime;
}

void setupLilygoMqtt()
{
    if (!motLoadAwsCredentials(awsCredentials)) {
        Serial.printf(
            "AWS IoT: credential load failed: %s\n",
            awsCredentials.message.c_str()
        );
        return;
    }

    if (!awsClient.begin(awsCredentials)) {
        Serial.printf(
            "AWS IoT: initialization failed: %s\n",
            awsClient.status().message.c_str()
        );
        return;
    }

    Serial.printf(
        "AWS IoT: shared transport enabled endpoint=%s port=%u "
        "clientId=%s vehicle=%s credentials=LittleFS\n",
        awsCredentials.endpoint.c_str(),
        awsCredentials.port,
        awsCredentials.thingName.c_str(),
        awsCredentials.vehicleId.c_str()
    );
}

void lilygoMqttLoop()
{
    requestUtcSyncIfPossible();

    awsClient.loop(
        awsRuntime(),
        WiFi.status() == WL_CONNECTED
    );

    if (!awsClient.connected()) return;

    if (millis() - lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS) {
        lastPublishMs = millis();
        publishLilygoTelemetry();
    }
}

void publishLilygoTelemetry()
{
    if (!awsClient.connected()) return;

    awsClient.publishFloat(
        "display/soc",
        telemetry.display.soc
    );
    awsClient.publishFloat(
        "display/speed_kmh",
        telemetry.display.speedKmh
    );
    awsClient.publishFloat(
        "display/odometer_km",
        telemetry.display.odometerKm
    );
    awsClient.publishInt(
        "display/estimated_range_km",
        telemetry.display.estimatedRangeKm
    );

    awsClient.publishBool(
        "charging/is_charging",
        telemetry.charging.isCharging
    );
    awsClient.publishBool(
        "charging/plugged",
        telemetry.charging.plugged
    );
    awsClient.publishInt(
        "charging/power_display",
        telemetry.charging.powerDisplay
    );
    awsClient.publishInt(
        "charging/power_signed",
        telemetry.charging.powerSigned
    );

    if (l76kGpsValid()) {
        awsClient.publishFloat(
            "location/latitude",
            static_cast<float>(l76kLatitude()),
            6
        );
        awsClient.publishFloat(
            "location/longitude",
            static_cast<float>(l76kLongitude()),
            6
        );
        awsClient.publishFloat(
            "location/speed_kmph",
            static_cast<float>(l76kSpeedKmph()),
            1
        );
        awsClient.publishInt(
            "location/satellites",
            static_cast<long>(l76kSatellites())
        );
        if (!isnan(l76kHdop())) {
            awsClient.publishFloat(
                "location/hdop",
                static_cast<float>(l76kHdop()),
                1
            );
        }
        awsClient.publishInt(
            "location/age_ms",
            static_cast<long>(l76kLocationAgeMs())
        );
    }

    awsClient.publish(
        "system/device_id",
        lilygoDeviceName(),
        true
    );
    awsClient.publish(
        "system/device_name",
        lilygoDeviceName(),
        true
    );
    awsClient.publish(
        "system/mqtt_client_id",
        awsCredentials.thingName,
        true
    );
    awsClient.publish(
        "system/firmware_version",
        telemetry.system.firmwareVersion,
        true
    );
    awsClient.publish(
        "system/ip_address",
        lilygoNetworkIp(),
        true
    );
    awsClient.publish(
        "system/network_mode",
        lilygoNetworkModeName(),
        true
    );
    awsClient.publish(
        "system/mqtt_transport",
        "WiFi",
        true
    );
    awsClient.publishInt(
        "system/wifi_rssi",
        WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
        true
    );
    awsClient.publishInt(
        "system/uptime_sec",
        millis() / 1000UL,
        true
    );
    awsClient.publishLastSeenUtc();
}

String lilygoMqttStatusJson()
{
    const MotAwsStatus& status = awsClient.status();

    String json = "{";
    json += "\"enabled\":" +
        String(awsClient.enabled() ? "true" : "false") + ",";
    json += "\"connected\":" +
        String(awsClient.connected() ? "true" : "false") + ",";
    json += "\"transport\":\"WiFi\",";
    json += "\"transportAvailable\":" +
        String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"wantedTransport\":\"WiFi\",";
    json += "\"mode\":\"AWS_IOT_X509_SHARED\",";
    json += "\"host\":\"" + esc(awsCredentials.endpoint) + "\",";
    json += "\"port\":" + String(awsCredentials.port) + ",";
    json += "\"credentialsLoaded\":" +
        String(awsCredentials.loaded ? "true" : "false") + ",";
    json += "\"clientId\":\"" + esc(awsCredentials.thingName) + "\",";
    json += "\"vehicleId\":\"" + esc(awsCredentials.vehicleId) + "\",";
    json += "\"state\":" + String(status.mqttState) + ",";
    json += "\"stateText\":\"" +
        String(mqttStateText(status.mqttState)) + "\",";
    json += "\"connectAttempts\":" +
        String(status.connectAttempts) + ",";
    json += "\"reconnectCount\":" +
        String(status.reconnectCount) + ",";
    json += "\"timeValid\":" +
        String(status.timeValid ? "true" : "false") + ",";
    json += "\"publishCount\":" +
        String(status.publishCount) + ",";
    json += "\"heartbeatCount\":" +
        String(status.heartbeatCount) + ",";
    json += "\"birthCount\":" +
        String(status.birthCount) + ",";
    json += "\"wifiIp\":\"" +
        esc(WiFi.localIP().toString()) + "\",";
    json += "\"lteIp\":\"" + esc(lilygoLteIp()) + "\",";
    json += "\"message\":\"" + esc(status.message) + "\"";
    json += "}";
    return json;
}

String lilygoMqttDebugJson()
{
    const MotAwsStatus& status = awsClient.status();

    String json = "{";
    json += "\"wifiConnected\":" +
        String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"networkMode\":\"" +
        esc(lilygoNetworkModeName()) + "\",";
    json += "\"wantedTransport\":\"WiFi\",";
    json += "\"activeTransport\":\"WiFi\",";
    json += "\"transportAvailable\":" +
        String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"mqttMode\":\"AWS_IOT_X509_SHARED\",";
    json += "\"mqttHost\":\"" +
        esc(awsCredentials.endpoint) + "\",";
    json += "\"mqttPort\":" + String(awsCredentials.port) + ",";
    json += "\"credentialsLoaded\":" +
        String(awsCredentials.loaded ? "true" : "false") + ",";
    json += "\"lteGprsConnected\":" +
        String(lilygoGprsConnected() ? "true" : "false") + ",";
    json += "\"lteIp\":\"" + esc(lilygoLteIp()) + "\",";
    json += "\"lteTcpConnected\":" +
        String(lilygoLteTcpConnected() ? "true" : "false") + ",";
    json += "\"mqttConnected\":" +
        String(awsClient.connected() ? "true" : "false") + ",";
    json += "\"mqttState\":" + String(status.mqttState) + ",";
    json += "\"mqttStateText\":\"" +
        String(mqttStateText(status.mqttState)) + "\",";
    json += "\"connectAttempts\":" +
        String(status.connectAttempts) + ",";
    json += "\"heartbeatCount\":" +
        String(status.heartbeatCount) + ",";
    json += "\"birthCount\":" +
        String(status.birthCount) + ",";
    json += "\"message\":\"" + esc(status.message) + "\"";
    json += "}";
    return json;
}

#else

#include <PubSubClient.h>
#include "lte/lilygo_lte_client.h"

static WiFiClient wifiClient;
static LilygoLteClient lteClient;
static PubSubClient mqtt;
static String activeTransport;
static unsigned long lastReconnectAttemptMs = 0;
static int lastConnectState = MQTT_DISCONNECTED;
static uint32_t connectAttempts = 0;
static String lastMessage;

static const unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000;
static const unsigned long MQTT_LTE_RECONNECT_INTERVAL_MS = 60000;

static bool mqttEnabled()
{
    String host = config.mqttHost;
    host.trim();
    return !host.isEmpty();
}

static String mqttClientId()
{
    String id = lilygoDeviceName();
    id.trim();
    id.toLowerCase();
    id.replace(" ", "-");
    id.replace("/", "-");
    if (id.isEmpty()) id = "mot-lilygo";
    if (!id.startsWith("mot-")) id = "mot-" + id;
    return id;
}

static String topic(const char* suffix)
{
    String prefix = config.mqttPrefix;
    String vehicle = config.vehicleId;

    prefix.trim();
    while (prefix.endsWith("/")) {
        prefix.remove(prefix.length() - 1);
    }
    if (prefix.isEmpty()) prefix = "mot";

    vehicle.trim();
    vehicle.replace("/", "-");
    if (vehicle.isEmpty()) vehicle = "pioneer";

    return prefix + "/" + vehicle + "/" + suffix;
}

static String wantedTransport()
{
    if (WiFi.status() == WL_CONNECTED) return "WiFi";
    if (lilygoGprsConnected()) return "LTE";
    return "";
}

static bool transportAvailable()
{
    return !wantedTransport().isEmpty();
}

static void selectTransport()
{
    const String wanted = wantedTransport();
    if (wanted == activeTransport) return;

    if (mqtt.connected()) mqtt.disconnect();

    if (wanted == "WiFi") {
        mqtt.setClient(wifiClient);
        activeTransport = "WiFi";
        Serial.println("Legacy MQTT: transport selected WiFi");
    } else if (wanted == "LTE") {
        mqtt.setClient(lteClient);
        activeTransport = "LTE";
        Serial.println("Legacy MQTT: transport selected LTE");
    } else {
        activeTransport = "";
        lastMessage = "MQTT transport unavailable";
    }

    lastReconnectAttemptMs = 0;
}

static void reconnectMqtt()
{
    if (!mqttEnabled()) {
        lastMessage = "MQTT disabled: no host configured";
        return;
    }

    selectTransport();
    if (!transportAvailable() || mqtt.connected()) return;

    const unsigned long interval =
        activeTransport == "LTE"
            ? MQTT_LTE_RECONNECT_INTERVAL_MS
            : MQTT_RECONNECT_INTERVAL_MS;

    if (lastReconnectAttemptMs &&
        millis() - lastReconnectAttemptMs < interval) return;

    lastReconnectAttemptMs = millis();
    connectAttempts++;

    mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(
        activeTransport == "LTE" ? 30 : 10
    );

    Serial.printf(
        "Legacy MQTT %s: connecting host=%s port=%u clientId=%s\n",
        activeTransport.c_str(),
        config.mqttHost.c_str(),
        config.mqttPort,
        mqttClientId().c_str()
    );

    const bool ok = config.mqttUser.length()
        ? mqtt.connect(
            mqttClientId().c_str(),
            config.mqttUser.c_str(),
            config.mqttPass.c_str())
        : mqtt.connect(mqttClientId().c_str());

    lastConnectState = mqtt.state();
    lastMessage = ok
        ? "MQTT connected via " + activeTransport
        : "MQTT connect failed rc=" + String(lastConnectState);
}

static void publishLegacy(
    const char* suffix,
    const String& payload,
    bool retained = true
) {
    if (mqtt.connected()) {
        mqtt.publish(topic(suffix).c_str(), payload.c_str(), retained);
    }
}

static void publishLegacyFloat(
    const char* suffix,
    float value,
    int decimals = 1
) {
    if (isnan(value)) return;
    char buffer[40];
    dtostrf(value, 0, decimals, buffer);
    publishLegacy(suffix, buffer);
}

static void publishLegacyInt(const char* suffix, long value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%ld", value);
    publishLegacy(suffix, buffer);
}

static void publishLegacyBool(const char* suffix, bool value)
{
    publishLegacy(suffix, value ? "1" : "0");
}

void setupLilygoMqtt()
{
    mqtt.setClient(wifiClient);

    if (!mqttEnabled()) {
        Serial.println("Legacy MQTT: disabled");
        lastMessage = "MQTT disabled: no host configured";
        return;
    }

    mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
    Serial.printf(
        "Legacy MQTT: enabled host=%s port=%u clientId=%s\n",
        config.mqttHost.c_str(),
        config.mqttPort,
        mqttClientId().c_str()
    );
}

void lilygoMqttLoop()
{
    if (!mqttEnabled()) return;

    selectTransport();
    reconnectMqtt();
    mqtt.loop();

    if (mqtt.connected() &&
        millis() - lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS) {
        lastPublishMs = millis();
        publishLilygoTelemetry();
    }
}

void publishLilygoTelemetry()
{
    if (!mqtt.connected()) return;

    publishLegacyFloat("display/soc", telemetry.display.soc);
    publishLegacyFloat(
        "display/speed_kmh",
        telemetry.display.speedKmh
    );
    publishLegacyFloat(
        "display/odometer_km",
        telemetry.display.odometerKm
    );
    publishLegacyInt(
        "display/estimated_range_km",
        telemetry.display.estimatedRangeKm
    );

    publishLegacyBool(
        "charging/is_charging",
        telemetry.charging.isCharging
    );
    publishLegacyBool(
        "charging/plugged",
        telemetry.charging.plugged
    );
    publishLegacyInt(
        "charging/power_display",
        telemetry.charging.powerDisplay
    );
    publishLegacyInt(
        "charging/power_signed",
        telemetry.charging.powerSigned
    );

    if (l76kGpsValid()) {
        publishLegacyFloat(
            "location/latitude",
            static_cast<float>(l76kLatitude()),
            6
        );
        publishLegacyFloat(
            "location/longitude",
            static_cast<float>(l76kLongitude()),
            6
        );
        publishLegacyFloat(
            "location/speed_kmph",
            static_cast<float>(l76kSpeedKmph())
        );
        publishLegacyInt(
            "location/satellites",
            l76kSatellites()
        );
    }

    publishLegacy("system/device_id", lilygoDeviceName());
    publishLegacy("system/device_name", lilygoDeviceName());
    publishLegacy(
        "system/firmware_version",
        telemetry.system.firmwareVersion
    );
    publishLegacy("system/ip_address", lilygoNetworkIp());
    publishLegacy(
        "system/network_mode",
        lilygoNetworkModeName()
    );
    publishLegacy("system/mqtt_transport", activeTransport);
    publishLegacyInt(
        "system/wifi_rssi",
        WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0
    );
    publishLegacyInt(
        "system/uptime_sec",
        millis() / 1000UL
    );
}

String lilygoMqttStatusJson()
{
    String json = "{";
    json += "\"enabled\":" +
        String(mqttEnabled() ? "true" : "false") + ",";
    json += "\"connected\":" +
        String(mqtt.connected() ? "true" : "false") + ",";
    json += "\"transport\":\"" + esc(activeTransport) + "\",";
    json += "\"transportAvailable\":" +
        String(transportAvailable() ? "true" : "false") + ",";
    json += "\"wantedTransport\":\"" +
        esc(wantedTransport()) + "\",";
    json += "\"mode\":\"PLAIN_DEBUG\",";
    json += "\"host\":\"" + esc(config.mqttHost) + "\",";
    json += "\"port\":" + String(config.mqttPort) + ",";
    json += "\"clientId\":\"" + esc(mqttClientId()) + "\",";
    json += "\"state\":" + String(mqtt.state()) + ",";
    json += "\"stateText\":\"" +
        String(mqttStateText(mqtt.state())) + "\",";
    json += "\"lastConnectState\":" +
        String(lastConnectState) + ",";
    json += "\"connectAttempts\":" +
        String(connectAttempts) + ",";
    json += "\"wifiIp\":\"" +
        esc(WiFi.localIP().toString()) + "\",";
    json += "\"lteIp\":\"" + esc(lilygoLteIp()) + "\",";
    json += "\"message\":\"" + esc(lastMessage) + "\"";
    json += "}";
    return json;
}

String lilygoMqttDebugJson()
{
    return lilygoMqttStatusJson();
}

#endif
