#include "mqtt_client.h"
#include "../app_config.h"
#include "../network/wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

#include "telemetry/telemetry.h"
#include "system/device_id.h"
#include "../gps/wroom_gps.h"

#ifdef MOT_AWS_IOT
#include <MotAwsIot.h>

static MotAwsCredentials awsCredentials;
static MotAwsIotClient awsClient;

static MotAwsRuntime awsRuntime()
{
    MotAwsRuntime runtime;
    runtime.deviceId = motDeviceId();
    runtime.deviceName = config.deviceName;
    runtime.firmwareVersion = telemetry.system.firmwareVersion;
    runtime.networkMode = networkModeName();
    runtime.transport = "WiFi";
    runtime.ipAddress = networkIp();
    runtime.wifiRssi = networkRssi();
    runtime.uptimeSec = millis() / 1000UL;
    return runtime;
}

#else

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
    if (!networkOnline() || !config.mqttEnabled() || mqtt.connected()) return;

    Serial.print("Connecting legacy MQTT... ");
    String clientId = config.mqttClientId();
    if (mqtt.connect(
            clientId.c_str(),
            config.mqttUser.c_str(),
            config.mqttPass.c_str())) {
        Serial.println("connected");
    } else {
        Serial.printf("failed rc=%d\n", mqtt.state());
    }
}

#endif

void setupMqtt()
{
#ifdef MOT_AWS_IOT
    if (!motLoadAwsCredentials(awsCredentials)) {
        Serial.printf(
            "AWS IoT: credentials unavailable: %s\n",
            awsCredentials.message.c_str()
        );
        return;
    }

    awsClient.begin(awsCredentials);
    Serial.printf(
        "AWS IoT: configured endpoint=%s port=%u thing=%s vehicle=%s\n",
        awsCredentials.endpoint.c_str(),
        awsCredentials.port,
        awsCredentials.thingName.c_str(),
        awsCredentials.vehicleId.c_str()
    );
#else
    if (config.mqttEnabled()) {
        mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
        Serial.printf(
            "Legacy MQTT: enabled host=%s port=%u clientId=%s\n",
            config.mqttHost.c_str(),
            config.mqttPort,
            config.mqttClientId().c_str()
        );
    } else {
        Serial.println("Legacy MQTT: disabled");
    }
#endif
}

void mqttLoop()
{
#ifdef MOT_AWS_IOT
    awsClient.loop(awsRuntime(), networkOnline());
#else
    if (!config.mqttEnabled()) return;
    if (!mqtt.connected()) reconnectMqtt();
    mqtt.loop();
#endif
}

void publishTelemetry()
{
#ifdef MOT_AWS_IOT
    if (!awsClient.connected()) return;

    awsClient.publishFloat("display/soc", telemetry.display.soc);
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

    awsClient.publish(
        "system/device_id",
        motDeviceId(),
        true
    );
    awsClient.publish(
        "system/device_name",
        config.deviceName,
        true
    );
    awsClient.publish(
        "system/firmware_version",
        telemetry.system.firmwareVersion,
        true
    );
    awsClient.publish(
        "system/ip_address",
        networkIp(),
        true
    );
    awsClient.publish(
        "system/network_mode",
        networkModeName(),
        true
    );
    awsClient.publish(
        "system/mqtt_transport",
        "WiFi",
        true
    );
    awsClient.publishInt(
        "system/wifi_rssi",
        networkRssi(),
        true
    );
    awsClient.publishInt(
        "system/uptime_sec",
        millis() / 1000UL,
        true
    );
    awsClient.publishLastSeenUtc();

    // Preserve retained cloud coordinates when no current fix is available.
    // The backend derives "current" versus "last known" from receivedAt.
    if (wroomGpsValid()) {
        awsClient.publishFloat("location/latitude", static_cast<float>(wroomGpsLatitude()), 6, true);
        awsClient.publishFloat("location/longitude", static_cast<float>(wroomGpsLongitude()), 6, true);
        awsClient.publishFloat("location/speed_kmph", static_cast<float>(wroomGpsSpeedKmph()), 2, true);
        awsClient.publishInt("location/satellites", static_cast<long>(wroomGpsSatellites()), true);
        if (!isnan(wroomGpsHdop())) {
            awsClient.publishFloat("location/hdop", static_cast<float>(wroomGpsHdop()), 2, true);
        }
        awsClient.publishInt("location/age_ms", static_cast<long>(wroomGpsLocationAgeMs()), true);
    }

#else
    if (!config.mqttEnabled() || !mqtt.connected()) return;

    auto publishFloat = [](const char* suffix, float value, int decimals = 1) {
        if (isnan(value)) return;
        char buf[32];
        dtostrf(value, 0, decimals, buf);
        mqtt.publish(topic(suffix).c_str(), buf, true);
    };
    auto publishInt = [](const char* suffix, int value) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%d", value);
        mqtt.publish(topic(suffix).c_str(), buf, true);
    };
    auto publishBool = [](const char* suffix, bool value) {
        mqtt.publish(topic(suffix).c_str(), value ? "1" : "0", true);
    };

    publishFloat("display/soc", telemetry.display.soc);
    publishFloat("display/speed_kmh", telemetry.display.speedKmh);
    publishFloat("display/odometer_km", telemetry.display.odometerKm);
    publishInt(
        "display/estimated_range_km",
        telemetry.display.estimatedRangeKm
    );
    publishBool(
        "charging/is_charging",
        telemetry.charging.isCharging
    );
    publishBool("charging/plugged", telemetry.charging.plugged);
    publishInt(
        "charging/power_display",
        telemetry.charging.powerDisplay
    );
    publishInt(
        "charging/power_signed",
        telemetry.charging.powerSigned
    );

    if (wroomGpsValid()) {
        publishFloat("location/latitude", wroomGpsLatitude(), 6);
        publishFloat("location/longitude", wroomGpsLongitude(), 6);
        publishFloat("location/speed_kmph", wroomGpsSpeedKmph(), 2);
        publishInt("location/satellites", wroomGpsSatellites());
        publishFloat("location/hdop", wroomGpsHdop(), 2);
        publishInt("location/age_ms", wroomGpsLocationAgeMs());
    }
#endif
}
