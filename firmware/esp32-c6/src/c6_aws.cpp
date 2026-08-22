#include "c6_aws.h"

#include "c6_config.h"
#include "c6_gps.h"
#include "c6_network.h"
#include "c6_offline_cache.h"
#include "system/device_id.h"
#include "system/version.h"
#include "telemetry/telemetry.h"

#ifdef MOT_AWS_IOT
#include <MotAwsIot.h>

namespace {
MotAwsCredentials credentials;
MotAwsIotClient client;
uint32_t lastPublishMs = 0;
bool freshLivePublished = false;

MotAwsRuntime runtime()
{
    MotAwsRuntime value;
    value.deviceId = motDeviceId();
    value.deviceName = motHostname();
    value.firmwareVersion = MOT_VERSION;
    value.networkMode = "WiFi STA";
    value.transport = "WiFi";
    value.ipAddress = c6NetworkIp();
    value.wifiRssi = c6NetworkRssi();
    value.uptimeSec = millis() / 1000;
    return value;
}

bool publishTelemetry()
{
    if (!client.connected()) return false;
    bool published = false;
    const bool freshBmsCurrent = telemetry.bms.packCurrentValid &&
        millis() - telemetry.bms.packCurrentLastUpdateMs <= 10000;
    const bool freshBmsStatus = telemetry.bms.packStatusValid &&
        millis() - telemetry.bms.statusLastUpdateMs <= 10000;
    if (telemetry.display.valid) {
        published |= client.publishFloat("display/soc", telemetry.display.soc, 1);
        published |= client.publishFloat("display/speed_kmh", telemetry.display.speedKmh, 1);
        published |= client.publishFloat("display/odometer_km", telemetry.display.odometerKm, 1);
        published |= client.publishInt("display/estimated_range_km", telemetry.display.estimatedRangeKm);
    }
    if (freshBmsCurrent && freshBmsStatus) {
        published |= client.publishBool("charging/is_charging", telemetryIsCharging());
        published |= client.publishBool("charging/plugged", telemetry.bms.plugged);
        published |= client.publishInt("charging/power_signed",
                          static_cast<int>(lroundf(telemetry.bms.vehiclePowerW / 100.0f)));
    } else if (telemetry.charging.valid) {
        published |= client.publishBool("charging/is_charging", telemetryIsCharging());
        published |= client.publishBool("charging/plugged", telemetry.charging.plugged);
        published |= client.publishInt("charging/power_signed", telemetry.charging.powerSigned);
    }
    if (telemetry.bms.packStatusValid) {
        published |= client.publishFloat("bms/pack_voltage", telemetry.bms.packVoltageMv / 1000.0f, 3);
        published |= client.publishInt("bms/status_byte", telemetry.bms.statusByte);
    }
    if (telemetry.bms.packCurrentValid) {
        published |= client.publishFloat("bms/pack_current", telemetry.bms.packCurrentA, 1);
        published |= client.publishFloat("bms/pack_power_w", telemetry.bms.packPowerW, 0);
        published |= client.publishFloat("bms/vehicle_power_w", telemetry.bms.vehiclePowerW, 0);
        published |= client.publishBool("bms/is_regenerating", telemetry.bms.isRegenerating);
        published |= client.publishBool("bms/is_discharging", telemetry.bms.isDischarging);
    }
    if (telemetry.bms.cellVoltagesValid) {
        published |= client.publishInt("bms/cell_min_mv", telemetry.bms.minCellVoltageMv);
        published |= client.publishInt("bms/cell_max_mv", telemetry.bms.maxCellVoltageMv);
        published |= client.publishInt("bms/cell_delta_mv", telemetry.bms.cellVoltageDeltaMv);
    }
    if (c6GpsValid()) {
        published |= client.publishFloat("location/latitude", static_cast<float>(c6GpsLatitude()), 6);
        published |= client.publishFloat("location/longitude", static_cast<float>(c6GpsLongitude()), 6);
        published |= client.publishFloat("location/speed_kmph", static_cast<float>(c6GpsSpeedKmph()), 2);
        published |= client.publishInt("location/satellites", c6GpsSatellites());
    }
    return published;
}
}

void c6AwsSetup()
{
    if (!motLoadAwsCredentials(credentials)) {
        Serial.println("AWS IoT: " + credentials.message);
        return;
    }
    client.begin(credentials);
    client.setMessageCallback(c6OfflineCacheHandleAwsMessage);
    Serial.printf("AWS IoT: configured thing=%s vehicle=%s\n",
                  credentials.thingName.c_str(), credentials.vehicleId.c_str());
}

void c6AwsLoop()
{
    client.loop(runtime(), c6NetworkTransportReady());
    if (!client.connected()) freshLivePublished = false;
    if (client.connected() && millis() - lastPublishMs >= c6Config.publishIntervalMs) {
        lastPublishMs = millis();
        freshLivePublished = publishTelemetry() || freshLivePublished;
    }
    c6OfflineCacheLoop(client, client.connected(), freshLivePublished);
}

String c6AwsStatus()
{
    const MotAwsStatus &status = client.status();
    return String(status.connected ? "connected" : "disconnected") +
           " credentials=" + (status.credentialsLoaded ? "yes" : "no") +
           " publishes=" + String(status.publishCount) +
           " attempts=" + String(status.connectAttempts) +
           " failures=" + String(status.consecutiveConnectFailures) +
           " totalFailures=" + String(status.totalConnectFailures) +
           " lastConnectMs=" + String(status.lastConnectDurationMs) +
           " retryMs=" + String(status.reconnectDelayMs) +
           " message=" + status.message;
}
bool c6AwsConnected() { return client.connected(); }

#else
void c6AwsSetup() { Serial.println("AWS IoT: not included in this build"); }
void c6AwsLoop() {}
String c6AwsStatus() { return "not included in this build"; }
bool c6AwsConnected() { return true; }
#endif
