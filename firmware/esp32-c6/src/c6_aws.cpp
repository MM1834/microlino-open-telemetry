#include "c6_aws.h"

#include <ArduinoJson.h>
#include <math.h>
#include <time.h>

#include "c6_config.h"
#include "c6_charge_journal.h"
#include "c6_gps.h"
#include "c6_journey_energy.h"
#include "c6_network.h"
#include "c6_offline_cache.h"
#include "c6_telemetry_cadence.h"
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
bool successfulPublishSeen = false;
volatile bool endpointPrepared = false;
String preparedNetworkIp;
uint32_t lastSuccessfulPublishMs = 0;
uint32_t telemetrySequence = 0;
uint32_t lastCadenceEventGeneration = 0;
constexpr size_t TELEMETRY_ENVELOPE_MAX_BYTES = 4096;
constexpr uint32_t ALTITUDE_DISPLAY_MAX_AGE_MS = 3000;
constexpr float ALTITUDE_MIN_MOVING_SPEED_KMH = 1.0f;
#ifdef MOT_DISPLAY_BLE
portMUX_TYPE statusSnapshotMux = portMUX_INITIALIZER_UNLOCKED;
char statusSnapshot[1024] = "starting";
volatile bool connectedSnapshot = false;
volatile bool recentlyHealthySnapshot = false;
volatile bool allowsAbrpSnapshot = false;
volatile bool readyForTlsPauseSnapshot = false;
#endif
constexpr uint32_t AWS_RECENTLY_HEALTHY_MS = 30000;
constexpr uint32_t AWS_TLS_PAUSE_STABLE_MS = 5000;
uint32_t connectedObservedSinceMs = 0;

String uint64Text(uint64_t value)
{
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%llu",
             static_cast<unsigned long long>(value));
    return String(buffer);
}

bool altitudePublishable()
{
    const bool displayFresh = telemetry.display.valid &&
        static_cast<uint32_t>(millis() - telemetry.display.lastUpdateMs) <=
            ALTITUDE_DISPLAY_MAX_AGE_MS;
    const bool moving = displayFresh && isfinite(telemetry.display.speedKmh) &&
        telemetry.display.speedKmh > ALTITUDE_MIN_MOVING_SPEED_KMH;
    return c6GpsAltitudePublishable(moving);
}

void handleAwsMessage(char *topic, uint8_t *payload, unsigned int length)
{
    c6OfflineCacheHandleAwsMessage(topic, payload, length);
    c6JourneyBackfillHandleAwsMessage(topic, payload, length);
    c6ChargeBackfillHandleAwsMessage(topic, payload, length);
}

MotAwsRuntime runtime()
{
    MotAwsRuntime value;
    value.deviceId = motDeviceId();
    value.deviceName = motHostname();
    value.board = MOT_BOARD;
    value.firmwareVersion = MOT_VERSION;
    value.can1Profile = decoderProfileKey(c6Config.can1Profile);
    value.can2Profile = decoderProfileKey(c6Config.can2Profile);
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
    JsonDocument document;
    document["version"] = 1;
    document["vehicleId"] = credentials.vehicleId;
    document["deviceId"] = motDeviceId();
    document["sequence"] = ++telemetrySequence;
    document["sampledAt"] = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
    JsonObject values = document["values"].to<JsonObject>();
    const bool freshBmsCurrent = telemetry.bms.packCurrentValid &&
        millis() - telemetry.bms.packCurrentLastUpdateMs <= 10000;
    const bool freshBmsStatus = telemetry.bms.statusLastUpdateMs != 0 &&
        telemetry.bms.packStatusValid &&
        millis() - telemetry.bms.statusLastUpdateMs <= 10000;
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
    const bool journeyIncluded = c6JourneyEnergyAddToEnvelope(values);
    if (freshBmsCurrent && freshBmsStatus) {
        values["charging/is_charging"] = telemetryIsCharging();
        values["charging/plugged"] = telemetry.bms.plugged;
        values["charging/power_signed"] =
            static_cast<int>(lroundf(telemetry.bms.vehiclePowerW / 100.0f));
    } else if (telemetry.charging.valid) {
        values["charging/is_charging"] = telemetryIsCharging();
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
    if (c6GpsValid()) {
        values["location/latitude"] = round(c6GpsLatitude() * 1000000.0) / 1000000.0;
        values["location/longitude"] = round(c6GpsLongitude() * 1000000.0) / 1000000.0;
        values["location/speed_kmph"] = round(c6GpsSpeedKmph() * 100.0) / 100.0;
        if (altitudePublishable()) {
            values["location/altitude_m"] = round(c6GpsAltitudeMeters() * 10.0) / 10.0;
        }
        values["location/satellites"] = c6GpsSatellites();
    }
    String payload;
    payload.reserve(2048);
    serializeJson(document, payload);
    if (payload.length() > TELEMETRY_ENVELOPE_MAX_BYTES) return false;
    const bool published = client.publish("telemetry/state/v1", payload, true);
    if (published) {
        successfulPublishSeen = true;
        lastSuccessfulPublishMs = millis();
        if (journeyIncluded) c6JourneyEnergyEnvelopePublished();
    }
    return published;
}

String buildAwsStatus()
{
    if (!c6Config.motCloudEnabled) return "disabled; credentials retained";
    const MotAwsStatus &status = client.status();
    return String(status.connected ? "connected" : "disconnected") +
           " credentials=" + (status.credentialsLoaded ? "yes" : "no") +
           " publishes=" + String(status.publishCount) +
           " payloadBytes=" + uint64Text(status.payloadBytes) +
           " mqttPublishBytes=" + uint64Text(status.mqttPublishBytes) +
           " cadence=" + c6TelemetryCadenceMode() +
           " intervalMs=" + String(c6TelemetryCadenceIntervalMs(c6Config.publishIntervalMs)) +
           " attempts=" + String(status.connectAttempts) +
           " connects=" + String(status.reconnectCount) +
           " disconnects=" + String(status.disconnectCount) +
           " failures=" + String(status.consecutiveConnectFailures) +
           " totalFailures=" + String(status.totalConnectFailures) +
           " lastConnectMs=" + String(status.lastConnectDurationMs) +
           " retryMs=" + String(status.reconnectDelayMs) +
           " tlsCode=" + String(status.tlsErrorCode) +
           " tls=" + (status.tlsError.isEmpty() ? "none" : status.tlsError) +
           " message=" + status.message;
}

void refreshStatusSnapshot()
{
#ifdef MOT_DISPLAY_BLE
    const String value = buildAwsStatus();
    const bool connected = client.connected();
    const bool recentlyHealthy = successfulPublishSeen &&
        millis() - lastSuccessfulPublishMs <= AWS_RECENTLY_HEALTHY_MS;
    const bool allowsAbrp = !c6Config.motCloudEnabled || !client.enabled() || connected;
    const bool readyForTlsPause = !c6Config.motCloudEnabled || !client.enabled() ||
        (connected && connectedObservedSinceMs != 0 &&
         millis() - connectedObservedSinceMs >= AWS_TLS_PAUSE_STABLE_MS &&
         recentlyHealthy);
    portENTER_CRITICAL(&statusSnapshotMux);
    strlcpy(statusSnapshot, value.c_str(), sizeof(statusSnapshot));
    connectedSnapshot = connected;
    recentlyHealthySnapshot = recentlyHealthy;
    allowsAbrpSnapshot = allowsAbrp;
    readyForTlsPauseSnapshot = readyForTlsPause;
    portEXIT_CRITICAL(&statusSnapshotMux);
#endif
}
}

void c6AwsSetup()
{
    if (!c6Config.motCloudEnabled) {
        Serial.println("AWS IoT: MOT Cloud disabled; credentials retained");
        refreshStatusSnapshot();
        return;
    }
    if (!motLoadAwsCredentials(credentials)) {
        Serial.println("AWS IoT: " + credentials.message);
        refreshStatusSnapshot();
        return;
    }
    client.begin(credentials);
#ifdef MOT_DISPLAY_BLE
    // Keep the first connection's retained identity set compact as well. The
    // local display must remain responsive even on a marginal hotspot.
    client.setMinimalBirth(true);
#endif
    client.setMessageCallback(handleAwsMessage);
    Serial.printf("AWS IoT: configured thing=%s vehicle=%s\n",
                  credentials.thingName.c_str(), credentials.vehicleId.c_str());
    refreshStatusSnapshot();
}

void c6AwsLoop()
{
    if (!c6Config.motCloudEnabled) {
        if (client.connected()) client.disconnect();
        refreshStatusSnapshot();
        return;
    }
    client.loop(runtime(), c6NetworkTransportReady() && endpointPrepared);
    if (client.connected()) {
        if (!connectedObservedSinceMs) connectedObservedSinceMs = millis();
    } else {
        connectedObservedSinceMs = 0;
        freshLivePublished = false;
    }
    const uint32_t cadenceGeneration = c6TelemetryCadenceEventGeneration();
    const uint32_t intervalMs =
        c6TelemetryCadenceIntervalMs(c6Config.publishIntervalMs);
    const bool eventPending = cadenceGeneration != lastCadenceEventGeneration;
    if (client.connected() &&
        (!freshLivePublished || eventPending || millis() - lastPublishMs >= intervalMs)) {
        lastPublishMs = millis();
        const bool published = publishTelemetry();
        freshLivePublished = published || freshLivePublished;
        if (published) lastCadenceEventGeneration = cadenceGeneration;
    }
    c6OfflineCacheLoop(client, client.connected(), freshLivePublished);
    c6JourneyBackfillLoop(client, client.connected(), freshLivePublished);
    c6ChargeBackfillLoop(client, client.connected(), freshLivePublished);
    refreshStatusSnapshot();
}

void c6AwsPrepareNetwork()
{
    if (!c6Config.motCloudEnabled || !c6NetworkTransportReady()) {
        endpointPrepared = false;
        preparedNetworkIp = "";
        return;
    }
    const String networkIp = c6NetworkIp();
    if (endpointPrepared && preparedNetworkIp == networkIp) return;
    endpointPrepared = false;
    if (client.resolveEndpointOnLoopTask()) {
        preparedNetworkIp = networkIp;
        endpointPrepared = true;
        Serial.println("AWS IoT: endpoint resolved on loop task");
    }
}

void c6AwsDisconnect() { client.disconnect(); refreshStatusSnapshot(); }

String c6AwsStatus()
{
#ifdef MOT_DISPLAY_BLE
    char value[sizeof(statusSnapshot)] = {};
    portENTER_CRITICAL(&statusSnapshotMux);
    strlcpy(value, statusSnapshot, sizeof(value));
    portEXIT_CRITICAL(&statusSnapshotMux);
    return String(value);
#else
    return buildAwsStatus();
#endif
}
bool c6AwsConnected()
{
#ifdef MOT_DISPLAY_BLE
    return connectedSnapshot;
#else
    return client.connected();
#endif
}
bool c6AwsRecentlyHealthy()
{
#ifdef MOT_DISPLAY_BLE
    return recentlyHealthySnapshot;
#else
    return successfulPublishSeen &&
        millis() - lastSuccessfulPublishMs <= AWS_RECENTLY_HEALTHY_MS;
#endif
}
bool c6AwsAllowsAbrp()
{
#ifdef MOT_DISPLAY_BLE
    return allowsAbrpSnapshot;
#else
    return !c6Config.motCloudEnabled || !client.enabled() || client.connected();
#endif
}
bool c6AwsReadyForTlsPause()
{
#ifdef MOT_DISPLAY_BLE
    return readyForTlsPauseSnapshot;
#else
    if (!c6Config.motCloudEnabled || !client.enabled()) return true;
    return client.connected() && connectedObservedSinceMs != 0 &&
        millis() - connectedObservedSinceMs >= AWS_TLS_PAUSE_STABLE_MS &&
        successfulPublishSeen &&
        millis() - lastSuccessfulPublishMs <= AWS_RECENTLY_HEALTHY_MS;
#endif
}

#else
void c6AwsSetup() { Serial.println("AWS IoT: not included in this build"); }
void c6AwsLoop() {}
void c6AwsPrepareNetwork() {}
void c6AwsDisconnect() {}
String c6AwsStatus() { return "not included in this build"; }
bool c6AwsConnected() { return true; }
bool c6AwsRecentlyHealthy() { return true; }
bool c6AwsAllowsAbrp() { return true; }
bool c6AwsReadyForTlsPause() { return true; }
#endif
