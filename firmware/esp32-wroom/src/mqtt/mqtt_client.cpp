#include "mqtt_client.h"
#include "../app_config.h"
#include "../network/wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "telemetry/telemetry.h"
#include "system/device_id.h"

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

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
    if (!networkOnline() || config.mqttHost.isEmpty() || mqtt.connected()) return;

    Serial.print("Connecting MQTT... ");
    String clientId = motDeviceId();
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
    if (!config.mqttHost.isEmpty()) {
        mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
    }
}

void mqttLoop()
{
    if (!mqtt.connected()) reconnectMqtt();
    mqtt.loop();
}

void publishTelemetry()
{
    if (!mqtt.connected()) return;

    publishFloat("display/soc", telemetry.display.soc);
    publishFloat("display/speed_kmh", telemetry.display.speedKmh);
    publishFloat("display/odometer_km", telemetry.display.odometerKm);
    publishInt("display/estimated_range_km", telemetry.display.estimatedRangeKm);

    publishBool("charging/is_charging", telemetry.charging.isCharging);
    publishBool("charging/plugged", telemetry.charging.plugged);
    publishInt("charging/power_display", telemetry.charging.powerDisplay);
    publishInt("charging/power_signed", telemetry.charging.powerSigned);

    mqtt.publish(topic("system/device_id").c_str(), motDeviceId().c_str(), true);
    mqtt.publish(topic("system/firmware_version").c_str(), telemetry.system.firmwareVersion.c_str(), true);
    mqtt.publish(topic("system/ip_address").c_str(), telemetry.system.ipAddress.c_str(), true);
    mqtt.publish(topic("system/network_mode").c_str(), telemetry.system.networkMode.c_str(), true);
    publishInt("system/wifi_rssi", telemetry.system.wifiRssi);
    publishInt("system/uptime_sec", telemetry.system.uptimeSec);
}
