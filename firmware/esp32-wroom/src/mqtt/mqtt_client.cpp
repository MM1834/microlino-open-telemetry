#include "mqtt_client.h"
#include "../app_config.h"
#include "../network/wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "telemetry/telemetry.h"
#include "system/device_id.h"

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static String topic(const char *suffix);

static bool ntpSyncRequested = false;
static const time_t MIN_VALID_UTC = 1700000000;

static void requestUtcSyncIfPossible()
{
    if (ntpSyncRequested || WiFi.status() != WL_CONNECTED) return;

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ntpSyncRequested = true;
    Serial.println("UTC: NTP synchronization requested");
}

static bool publishLastSeenUtc()
{
    requestUtcSyncIfPossible();

    const time_t nowUtc = time(nullptr);
    if (nowUtc < MIN_VALID_UTC) return false;

    char value[24];
    snprintf(value, sizeof(value), "%lld", static_cast<long long>(nowUtc));
    return mqtt.publish(topic("system/last_seen_utc").c_str(), value, true);
}

static String topic(const char *suffix)
{
    String prefix = config.mqttPrefix;
    prefix.trim();
    while (prefix.endsWith("/")) {
        prefix.remove(prefix.length() - 1);
    }
    if (prefix.isEmpty()) prefix = "mot";

    String vehicle = config.vehicleId;
    vehicle.trim();
    vehicle.replace("/", "-");
    if (vehicle.isEmpty()) vehicle = "pioneer";

    return prefix + "/" + vehicle + "/" + suffix;
}

static void reconnectMqtt()
{
    if (!networkOnline() || !config.mqttEnabled() || mqtt.connected()) return;

    Serial.print("Connecting MQTT... ");
    String clientId = config.mqttClientId();
    if (mqtt.connect(clientId.c_str(), config.mqttUser.c_str(), config.mqttPass.c_str())) {
        Serial.println("connected");
    } else {
        Serial.printf("failed rc=%d\n", mqtt.state());
    }
}

static void publishFloat(const char *suffix, float value, int decimals = 1)
{
    if (isnan(value)) return;
    char buf[32];
    dtostrf(value, 0, decimals, buf);
    mqtt.publish(topic(suffix).c_str(), buf, true);
}

static void publishInt(const char *suffix, int value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", value);
    mqtt.publish(topic(suffix).c_str(), buf, true);
}

static void publishBool(const char *suffix, bool value)
{
    mqtt.publish(topic(suffix).c_str(), value ? "1" : "0", true);
}

void setupMqtt()
{
    if (config.mqttEnabled()) {
        mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
        Serial.printf("MQTT: enabled host=%s port=%u clientId=%s\n", config.mqttHost.c_str(), config.mqttPort, config.mqttClientId().c_str());
    } else {
        Serial.println("MQTT: disabled (no host configured)");
    }
}

void mqttLoop()
{
    requestUtcSyncIfPossible();
    if (!config.mqttEnabled()) return;
    if (!mqtt.connected()) reconnectMqtt();
    mqtt.loop();
}

void publishTelemetry()
{
    if (!config.mqttEnabled() || !mqtt.connected()) return;

    publishFloat("display/soc", telemetry.display.soc);
    publishFloat("display/speed_kmh", telemetry.display.speedKmh);
    publishFloat("display/odometer_km", telemetry.display.odometerKm);
    publishInt("display/estimated_range_km", telemetry.display.estimatedRangeKm);

    publishBool("charging/is_charging", telemetry.charging.isCharging);
    publishBool("charging/plugged", telemetry.charging.plugged);
    publishInt("charging/power_display", telemetry.charging.powerDisplay);
    publishInt("charging/power_signed", telemetry.charging.powerSigned);

    mqtt.publish(topic("system/device_id").c_str(), motDeviceId().c_str(), true);
    mqtt.publish(topic("system/device_name").c_str(), config.deviceName.c_str(), true);
    mqtt.publish(topic("system/mqtt_client_id").c_str(), config.mqttClientId().c_str(), true);
    mqtt.publish(topic("system/firmware_version").c_str(), telemetry.system.firmwareVersion.c_str(), true);
    mqtt.publish(topic("system/ip_address").c_str(), telemetry.system.ipAddress.c_str(), true);
    mqtt.publish(topic("system/network_mode").c_str(), telemetry.system.networkMode.c_str(), true);
    publishInt("system/wifi_rssi", telemetry.system.wifiRssi);
    publishInt("system/uptime_sec", telemetry.system.uptimeSec);
    publishLastSeenUtc();
}
