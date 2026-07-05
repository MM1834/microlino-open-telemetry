#include "lilygo_mqtt.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "config/lilygo_config.h"
#include "network/lilygo_network.h"
#include "gps/l76k_gps.h"
#include "telemetry/telemetry.h"

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static unsigned long lastReconnectAttemptMs = 0;
static unsigned long lastPublishMs = 0;
static uint32_t publishCount = 0;
static uint32_t connectAttempts = 0;
static int lastConnectState = 0;
static String lastMessage = "";

static const unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000;
static const unsigned long MQTT_PUBLISH_INTERVAL_MS = 5000;

static String esc(String s)
{
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    s.replace("\r", "\\r");
    s.replace("\n", "\\n");
    return s;
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

static String topic(const char *suffix)
{
    String prefix = config.mqttPrefix;
    prefix.trim();
    while (prefix.endsWith("/")) prefix.remove(prefix.length() - 1);
    if (prefix.isEmpty()) prefix = "mot";

    String vehicle = config.vehicleId;
    vehicle.trim();
    vehicle.replace("/", "-");
    if (vehicle.isEmpty()) vehicle = "pioneer";

    return prefix + "/" + vehicle + "/" + suffix;
}

static bool wifiTransportAvailable()
{
    return WiFi.status() == WL_CONNECTED;
}

static void reconnectMqtt()
{
    if (!mqttEnabled()) {
        lastMessage = "MQTT disabled: no host configured";
        return;
    }

    if (!wifiTransportAvailable()) {
        if (mqtt.connected()) mqtt.disconnect();
        lastMessage = "MQTT transport unavailable: WiFi not active";
        return;
    }

    if (mqtt.connected()) return;

    if (millis() - lastReconnectAttemptMs < MQTT_RECONNECT_INTERVAL_MS) return;
    lastReconnectAttemptMs = millis();
    connectAttempts++;

    mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(5);

    Serial.printf("MQTT WiFi: connecting host=%s port=%u clientId=%s ip=%s\n",
                  config.mqttHost.c_str(),
                  config.mqttPort,
                  mqttClientId().c_str(),
                  WiFi.localIP().toString().c_str());

    bool ok;
    if (config.mqttUser.length()) {
        ok = mqtt.connect(mqttClientId().c_str(), config.mqttUser.c_str(), config.mqttPass.c_str());
    } else {
        ok = mqtt.connect(mqttClientId().c_str());
    }

    lastConnectState = mqtt.state();

    if (ok) {
        lastMessage = "MQTT connected via WiFi";
        Serial.println("MQTT WiFi: connected");
    } else {
        lastMessage = "MQTT connect failed rc=" + String(lastConnectState) + " (" + mqttStateText(lastConnectState) + ")";
        Serial.printf("MQTT WiFi: failed rc=%d (%s)\n", lastConnectState, mqttStateText(lastConnectState));
    }
}

static void publishFloat(const char *suffix, float value, int decimals = 1)
{
    if (!mqtt.connected() || isnan(value)) return;
    char buf[32];
    dtostrf(value, 0, decimals, buf);
    mqtt.publish(topic(suffix).c_str(), buf, true);
}

static void publishDouble(const char *suffix, double value, int decimals = 6)
{
    if (!mqtt.connected() || isnan(value)) return;
    char buf[40];
    dtostrf(value, 0, decimals, buf);
    mqtt.publish(topic(suffix).c_str(), buf, true);
}

static void publishInt(const char *suffix, int value)
{
    if (!mqtt.connected()) return;
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", value);
    mqtt.publish(topic(suffix).c_str(), buf, true);
}

static void publishBool(const char *suffix, bool value)
{
    if (!mqtt.connected()) return;
    mqtt.publish(topic(suffix).c_str(), value ? "1" : "0", true);
}

void setupLilygoMqtt()
{
    if (mqttEnabled()) {
        mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
        mqtt.setKeepAlive(30);
        mqtt.setSocketTimeout(5);
        Serial.printf("MQTT: enabled host=%s port=%u clientId=%s transport=WiFi only\n",
                      config.mqttHost.c_str(), config.mqttPort, mqttClientId().c_str());
        lastMessage = "MQTT enabled; waiting for WiFi";
    } else {
        Serial.println("MQTT: disabled (no host configured)");
        lastMessage = "MQTT disabled: no host configured";
    }
}

void lilygoMqttLoop()
{
    if (!mqttEnabled()) return;

    if (!wifiTransportAvailable()) {
        if (mqtt.connected()) mqtt.disconnect();
        lastMessage = "MQTT transport unavailable: WiFi not active";
        return;
    }

    reconnectMqtt();
    mqtt.loop();

    if (mqtt.connected() && millis() - lastPublishMs > MQTT_PUBLISH_INTERVAL_MS) {
        lastPublishMs = millis();
        publishLilygoTelemetry();
    }
}

void publishLilygoTelemetry()
{
    if (!mqttEnabled() || !mqtt.connected() || !wifiTransportAvailable()) return;

    publishFloat("display/soc", telemetry.display.soc);
    publishFloat("display/speed_kmh", telemetry.display.speedKmh);
    publishFloat("display/odometer_km", telemetry.display.odometerKm);
    publishInt("display/estimated_range_km", telemetry.display.estimatedRangeKm);

    publishBool("charging/is_charging", telemetry.charging.isCharging);
    publishBool("charging/plugged", telemetry.charging.plugged);
    publishInt("charging/power_display", telemetry.charging.powerDisplay);
    publishInt("charging/power_signed", telemetry.charging.powerSigned);

    if (l76kGpsValid()) {
        publishDouble("location/latitude", l76kLatitude(), 6);
        publishDouble("location/longitude", l76kLongitude(), 6);
        publishFloat("location/speed_kmph", (float)l76kSpeedKmph(), 1);
        publishInt("location/satellites", (int)l76kSatellites());
        if (!isnan(l76kHdop())) publishFloat("location/hdop", (float)l76kHdop(), 1);
        publishInt("location/age_ms", (int)l76kLocationAgeMs());
    }

    mqtt.publish(topic("system/device_id").c_str(), lilygoDeviceName().c_str(), true);
    mqtt.publish(topic("system/device_name").c_str(), lilygoDeviceName().c_str(), true);
    mqtt.publish(topic("system/mqtt_client_id").c_str(), mqttClientId().c_str(), true);
    mqtt.publish(topic("system/firmware_version").c_str(), telemetry.system.firmwareVersion.c_str(), true);
    mqtt.publish(topic("system/ip_address").c_str(), telemetry.system.ipAddress.c_str(), true);
    mqtt.publish(topic("system/network_mode").c_str(), telemetry.system.networkMode.c_str(), true);
    publishInt("system/wifi_rssi", telemetry.system.wifiRssi);
    publishInt("system/uptime_sec", telemetry.system.uptimeSec);

    publishCount++;
    lastMessage = "Telemetry published via WiFi";
}

String lilygoMqttStatusJson()
{
    String json = "{";
    json += "\"enabled\":" + String(mqttEnabled() ? "true" : "false") + ",";
    json += "\"connected\":" + String(mqtt.connected() ? "true" : "false") + ",";
    json += "\"transport\":\"WiFi\",";
    json += "\"transportAvailable\":" + String(wifiTransportAvailable() ? "true" : "false") + ",";
    json += "\"host\":\"" + esc(config.mqttHost) + "\",";
    json += "\"port\":" + String(config.mqttPort) + ",";
    json += "\"clientId\":\"" + esc(mqttClientId()) + "\",";
    json += "\"state\":" + String(mqtt.state()) + ",";
    json += "\"stateText\":\"" + String(mqttStateText(mqtt.state())) + "\",";
    json += "\"lastConnectState\":" + String(lastConnectState) + ",";
    json += "\"lastConnectStateText\":\"" + String(mqttStateText(lastConnectState)) + "\",";
    json += "\"connectAttempts\":" + String(connectAttempts) + ",";
    json += "\"publishCount\":" + String(publishCount) + ",";
    json += "\"wifiIp\":\"" + esc(WiFi.localIP().toString()) + "\",";
    json += "\"message\":\"" + esc(lastMessage) + "\"";
    json += "}";
    return json;
}


String lilygoMqttDebugJson()
{
    const unsigned long start = millis();

    WiFiClient testClient;
    bool tcpOk = false;

    String host = config.mqttHost;
    host.trim();

    if (!host.isEmpty()) {
        tcpOk = testClient.connect(host.c_str(), config.mqttPort);
        testClient.stop();
    }

    const unsigned long durationMs = millis() - start;

    String json = "{";
    json += "\"wifiStatus\":" + String((int)WiFi.status()) + ",";
    json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"ssid\":\"" + esc(WiFi.SSID()) + "\",";
    json += "\"localIp\":\"" + esc(WiFi.localIP().toString()) + "\",";
    json += "\"gateway\":\"" + esc(WiFi.gatewayIP().toString()) + "\",";
    json += "\"subnet\":\"" + esc(WiFi.subnetMask().toString()) + "\",";
    json += "\"dns0\":\"" + esc(WiFi.dnsIP(0).toString()) + "\",";
    json += "\"dns1\":\"" + esc(WiFi.dnsIP(1).toString()) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"networkMode\":\"" + esc(lilygoNetworkModeName()) + "\",";
    json += "\"transportAvailable\":" + String(wifiTransportAvailable() ? "true" : "false") + ",";
    json += "\"mqttHost\":\"" + esc(host) + "\",";
    json += "\"mqttPort\":" + String(config.mqttPort) + ",";
    json += "\"tcpConnectOk\":" + String(tcpOk ? "true" : "false") + ",";
    json += "\"tcpConnectMs\":" + String(durationMs) + ",";
    json += "\"mqttConnected\":" + String(mqtt.connected() ? "true" : "false") + ",";
    json += "\"mqttState\":" + String(mqtt.state()) + ",";
    json += "\"mqttStateText\":\"" + String(mqttStateText(mqtt.state())) + "\",";
    json += "\"lastConnectState\":" + String(lastConnectState) + ",";
    json += "\"lastConnectStateText\":\"" + String(mqttStateText(lastConnectState)) + "\",";
    json += "\"connectAttempts\":" + String(connectAttempts) + ",";
    json += "\"message\":\"" + esc(lastMessage) + "\"";
    json += "}";

    return json;
}

