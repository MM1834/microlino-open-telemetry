#include "c6_abrp.h"

#include "abrp/abrp_client.h"
#include "c6_config.h"
#include "c6_aws.h"
#include "c6_gps.h"
#include "c6_network.h"
#include "c6_telemetry_cadence.h"
#include "c6_cloud_worker.h"

namespace {
constexpr uint32_t ABRP_ACTIVE_INTERVAL_MS = 60000;
uint32_t lastCadenceEventGeneration = 0;
bool sendRequested = false;
bool sendStarted = false;
bool manualRequested = false;

AbrpSettings settings()
{
    AbrpSettings value;
    value.enabled = c6Config.abrpEnabled;
    value.apiKey = c6Config.abrpApiKey;
    value.userToken = c6Config.abrpUserToken;
    value.intervalMs = c6TelemetryCadenceIntervalMs(ABRP_ACTIVE_INTERVAL_MS);
    return value;
}

bool location(AbrpLocation &value)
{
    value.valid = c6GpsValid();
    if (value.valid) {
        value.latitude = c6GpsLatitude();
        value.longitude = c6GpsLongitude();
    }
    return value.valid;
}
}

void c6AbrpSetup() { setupAbrp(settings()); }
void c6AbrpLoop()
{
    const AbrpSettings current = settings();
    if (!abrpEnabled(current)) {
#ifdef MOT_DISPLAY_BLE
        if (sendRequested || sendStarted) {
            sendRequested = false;
            sendStarted = false;
            c6CloudWorkerReleaseAbrpPause();
        }
#endif
        manualRequested = false;
        return;
    }
    // When MOT Cloud is enabled and provisioned, do not start a second TLS
    // session while AWS MQTT is reconnecting or recovering memory. ABRP remains
    // independent when MOT Cloud is disabled or AWS is not provisioned.
    if (!c6NetworkTransportReady()) return;
    if (!abrpPrepareNetwork()) return;
    const uint32_t cadenceGeneration = c6TelemetryCadenceEventGeneration();
#ifndef MOT_DISPLAY_BLE
    if (manualRequested) {
        manualRequested = false;
        queueAbrpTelemetry(current, location);
        return;
    }
    if (cadenceGeneration != lastCadenceEventGeneration) {
        queueAbrpTelemetry(current, location);
        lastCadenceEventGeneration = cadenceGeneration;
        return;
    }
    abrpLoop(current, location);
    return;
#else
    if (!sendRequested && !c6AwsReadyForTlsPause()) return;
    if (!sendRequested && (manualRequested ||
        cadenceGeneration != lastCadenceEventGeneration ||
        abrpTelemetryDue(current))) {
        sendRequested = true;
        sendStarted = false;
        manualRequested = false;
        c6CloudWorkerRequestAbrpPause();
        lastCadenceEventGeneration = cadenceGeneration;
        return;
    }
    if (!sendRequested) return;
    if (!sendStarted) {
        if (!c6CloudWorkerAbrpPauseReady()) return;
        sendStarted = queueAbrpTelemetry(current, location);
        if (!sendStarted) {
            sendRequested = false;
            c6CloudWorkerReleaseAbrpPause();
        }
        return;
    }
    if (!abrpInFlight()) {
        sendRequested = false;
        sendStarted = false;
        c6CloudWorkerReleaseAbrpPause();
    }
#endif
}
bool c6AbrpQueueTelemetry()
{
    if (!c6NetworkTransportReady() || sendRequested) return false;
    manualRequested = true;
    return true;
}
String c6AbrpStatusJson() { return abrpStatusJson(settings()); }
bool c6AbrpConfigured() { return abrpEnabled(settings()); }
