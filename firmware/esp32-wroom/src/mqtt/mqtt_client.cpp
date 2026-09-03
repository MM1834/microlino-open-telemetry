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
    if (!config.awsEnabled()) { Serial.println("AWS IoT: disabled by service configuration"); return; }
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
    if (!config.awsEnabled()) return;
    awsClient.loop(awsRuntime(), networkOnline());
#else
    if (!config.mqttEnabled()) return;
    if (!mqtt.connected()) reconnectMqtt();
    mqtt.loop();
#endif
}

void publishTelemetry()
{
    const bool freshBmsCurrent = telemetry.bms.packCurrentValid &&
        millis() - telemetry.bms.packCurrentLastUpdateMs <= 10000;
    const bool freshBmsStatus = telemetry.bms.packStatusValid &&
        millis() - telemetry.bms.statusLastUpdateMs <= 10000;
#ifdef MOT_AWS_IOT
    if (!config.awsEnabled() || !awsClient.connected()) return;

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

    awsClient.publishBool("charging/is_charging", telemetryIsCharging());
    awsClient.publishBool("charging/plugged",
        freshBmsStatus ? telemetry.bms.plugged : telemetry.charging.plugged);
    awsClient.publishInt(
        "charging/power_display",
        telemetry.charging.powerDisplay
    );
    awsClient.publishInt(
        "charging/power_signed",
        freshBmsCurrent
            ? static_cast<int>(lroundf(telemetry.bms.vehiclePowerW / 100.0f))
            : telemetry.charging.powerSigned
    );
    if (telemetry.bms.packStatusValid) {
        awsClient.publishFloat("bms/pack_voltage", telemetry.bms.packVoltageMv / 1000.0f, 3);
        awsClient.publishInt("bms/status_byte", telemetry.bms.statusByte);
    }
    if (telemetry.bms.pioneerSocValid) {
        awsClient.publishFloat("bms/soc_internal", telemetry.bms.socInternal, 1);
        awsClient.publishFloat("bms/soc_display", telemetry.bms.socDisplay, 1);
    }
    if (telemetry.bms.standardSocValid)
        awsClient.publishFloat("bms/standard_soc", telemetry.bms.socPercent, 2);
    if (telemetry.bms.sohValid)
        awsClient.publishFloat("bms/soh_percent", telemetry.bms.sohPercent, 2);
    if (telemetry.bms.packCurrentValid) {
        awsClient.publishFloat("bms/pack_current", telemetry.bms.packCurrentA, 1);
        awsClient.publishFloat("bms/pack_power_w", telemetry.bms.packPowerW, 0);
        awsClient.publishFloat("bms/vehicle_power_w", telemetry.bms.vehiclePowerW, 0);
        awsClient.publishBool("bms/is_regenerating", telemetry.bms.isRegenerating);
        awsClient.publishBool("bms/is_discharging", telemetry.bms.isDischarging);
    }
    if (telemetry.bms.cellVoltagesValid) {
        awsClient.publishInt("bms/cell_min_mv", telemetry.bms.minCellVoltageMv);
        awsClient.publishInt("bms/cell_max_mv", telemetry.bms.maxCellVoltageMv);
        awsClient.publishInt("bms/cell_delta_mv", telemetry.bms.cellVoltageDeltaMv);
    }

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
    publishBool("charging/is_charging", telemetryIsCharging());
    publishBool("charging/plugged",
        freshBmsStatus ? telemetry.bms.plugged : telemetry.charging.plugged);
    publishInt(
        "charging/power_display",
        telemetry.charging.powerDisplay
    );
    publishInt(
        "charging/power_signed",
        freshBmsCurrent
            ? static_cast<int>(lroundf(telemetry.bms.vehiclePowerW / 100.0f))
            : telemetry.charging.powerSigned
    );
    if (telemetry.bms.packStatusValid) {
        publishFloat("bms/pack_voltage", telemetry.bms.packVoltageMv / 1000.0f, 3);
        publishInt("bms/status_byte", telemetry.bms.statusByte);
    }
    if (telemetry.bms.pioneerSocValid) {
        publishFloat("bms/soc_internal", telemetry.bms.socInternal, 1);
        publishFloat("bms/soc_display", telemetry.bms.socDisplay, 1);
    }
    if (telemetry.bms.standardSocValid)
        publishFloat("bms/standard_soc", telemetry.bms.socPercent, 2);
    if (telemetry.bms.sohValid)
        publishFloat("bms/soh_percent", telemetry.bms.sohPercent, 2);
    if (telemetry.bms.packCurrentValid) {
        publishFloat("bms/pack_current", telemetry.bms.packCurrentA);
        publishFloat("bms/pack_power_w", telemetry.bms.packPowerW, 0);
        publishFloat("bms/vehicle_power_w", telemetry.bms.vehiclePowerW, 0);
        publishBool("bms/is_regenerating", telemetry.bms.isRegenerating);
        publishBool("bms/is_discharging", telemetry.bms.isDischarging);
    }
    if (telemetry.bms.cellVoltagesValid) {
        publishInt("bms/cell_min_mv", telemetry.bms.minCellVoltageMv);
        publishInt("bms/cell_max_mv", telemetry.bms.maxCellVoltageMv);
        publishInt("bms/cell_delta_mv", telemetry.bms.cellVoltageDeltaMv);
    }

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

bool mqttTransportConnected()
{
#ifdef MOT_AWS_IOT
    return awsClient.connected();
#else
    return mqtt.connected();
#endif
}

MqttDiagResult mqttTransportDiagnostics()
{
#ifdef MOT_AWS_IOT
    const MotAwsStatus& status = awsClient.status();
    const bool connected = awsClient.connected();
    MqttDiagResult result;
    result.mode = "AWS_IOT_X509";
    result.enabled = config.awsEnabled();
    result.configured = awsCredentials.loaded;
    result.host = awsCredentials.endpoint;
    result.port = awsCredentials.port;
    result.wifiConnected = networkOnline();
    // An established AWS MQTT/TLS session proves successful endpoint resolution
    // and TCP/TLS connectivity without opening a second diagnostic connection.
    result.dnsOk = connected;
    result.tcpOk = connected;
    result.mqttOk = connected;
    result.mqttState = status.mqttState;
    result.message = status.message;
    return result;
#else
    MqttDiagResult result = MqttDiagnostics::test(
        config.mqttHost,
        config.mqttPort,
        config.mqttUser,
        config.mqttPass,
        "mot-health"
    );
    result.enabled = config.mqttEnabled();
    result.configured = !config.mqttHost.isEmpty() && config.mqttPort > 0;
    return result;
#endif
}
