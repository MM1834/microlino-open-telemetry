#include "c6_journey_energy.h"

#include "telemetry/telemetry.h"

#ifdef MOT_JOURNEY_ENERGY_COUNTER

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <MotAwsIot.h>
#include <esp_system.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "c6_config.h"
#include "c6_gps.h"
#include "system/version.h"

namespace {
constexpr float MOVING_SPEED_KMH = 1.0f;
constexpr uint32_t DISPLAY_FRESH_MS = 3000;
constexpr uint32_t POWER_FRESH_MS = 3000;
constexpr uint32_t MAX_INTEGRATION_STEP_MS = 2000;
constexpr uint32_t JOURNEY_STOP_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t CHECKPOINT_MS = 60000;
constexpr double WATT_MS_PER_WH = 3600000.0;
constexpr time_t MIN_VALID_UTC = 1700000000;
constexpr char JOURNEY_PATH[] = "/offline-cache/journeys.bin";
constexpr char JOURNEY_NEXT_PATH[] = "/offline-cache/journeys.next";
constexpr char ACTIVE_PATH[] = "/offline-cache/journey.active";
constexpr uint32_t JOURNEY_MAGIC = 0x4D4F544A;
constexpr uint8_t JOURNEY_VERSION = 1;
constexpr size_t JOURNEY_MAX_RECORDS = 32;
constexpr uint32_t BACKFILL_RETRY_MS = 30000;
constexpr uint32_t ALTITUDE_SAMPLE_MS = 30000;
constexpr float ALTITUDE_DEADBAND_M = 5.0f;
constexpr float ALTITUDE_MAX_RATE_MPS = 5.0f;

struct JourneyRecord {
    uint32_t magic;
    uint8_t version;
    uint8_t completionReason;
    uint8_t hadConnectivityGap;
    uint8_t reserved;
    char journeyId[40];
    int64_t startedAtMs;
    int64_t endedAtMs;
    int64_t lastMovingAtMs;
    float startOdometerKm;
    float endOdometerKm;
    float startSoc;
    float endSoc;
    uint32_t durationSeconds;
    uint32_t energyDrawnWh;
    uint32_t energyRegenWh;
    uint32_t powerCoverageSeconds;
    uint16_t odometerAnomalyCount;
    // Reuses the original reserved field, keeping version-1 journal records
    // binary-compatible. Older records read this as zero.
    uint16_t regenPeakW;
    uint32_t checksum;
};

static_assert(sizeof(JourneyRecord) <= 128, "Journey journal record must remain compact");

struct JourneyEnergyState {
    bool active = false;
    bool moving = false;
    bool sealed = false;
    bool publishPending = false;
    bool restartAfterPublish = false;
    uint32_t bootNonce = 0;
    uint32_t journeySequence = 0;
    uint32_t stoppedSinceMs = 0;
    uint32_t lastIntegrationMs = 0;
    uint32_t lastPublishMs = 0;
    double drawnWattMs = 0.0;
    double regenWattMs = 0.0;
    uint16_t regenPeakW = 0;
    int64_t startedAtMs = 0;
    uint32_t startedAtUptimeMs = 0;
    int64_t lastMovingAtMs = 0;
    float startOdometerKm = NAN;
    float endOdometerKm = NAN;
    float startSoc = NAN;
    float endSoc = NAN;
    uint32_t powerCoverageMs = 0;
    uint16_t odometerAnomalyCount = 0;
    bool altitudeValid = false;
    bool altitudeAnchorValid = false;
    uint8_t altitudeSampleCount = 0;
    float altitudeMeters = NAN;
    float altitudeAnchorMeters = NAN;
    float altitudeAscentMeters = 0.0f;
    float altitudeDescentMeters = 0.0f;
    uint32_t altitudeAcceptedMs = 0;
    uint32_t altitudeSampleMs = 0;
    bool cloudConnected = false;
    bool hadConnectivityGap = false;
    uint32_t lastCheckpointMs = 0;
    char counterId[40] = {};
};

JourneyEnergyState state;

struct BackfillState {
    bool mounted = false;
    bool subscribed = false;
    bool previousConnected = false;
    bool waitingForAck = false;
    bool blocked = false;
    uint32_t pending = 0;
    uint32_t replayed = 0;
    uint32_t duplicates = 0;
    uint32_t rejected = 0;
    uint32_t corrupt = 0;
    uint32_t dropped = 0;
    uint32_t lastAttemptMs = 0;
    String batchId;
    String lastResult = "none";
};

BackfillState backfill;

long drawnWh();
long regenWh();

uint32_t recordChecksum(const JourneyRecord &record)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < offsetof(JourneyRecord, checksum); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

bool validRecord(const JourneyRecord &record, bool activeAllowed = false)
{
    return record.magic == JOURNEY_MAGIC && record.version == JOURNEY_VERSION &&
        record.journeyId[0] != '\0' &&
        record.startedAtMs >= static_cast<int64_t>(MIN_VALID_UTC) * 1000 &&
        (activeAllowed ? record.endedAtMs == 0 : record.endedAtMs >= record.startedAtMs) &&
        isfinite(record.startOdometerKm) && isfinite(record.endOdometerKm) &&
        isfinite(record.startSoc) && isfinite(record.endSoc) &&
        record.checksum == recordChecksum(record);
}

bool currentUtcMs(int64_t &value)
{
    const time_t now = time(nullptr);
    if (now < MIN_VALID_UTC) return false;
    value = static_cast<int64_t>(now) * 1000;
    return true;
}

JourneyRecord snapshot(bool finalized, uint8_t reason = 0)
{
    JourneyRecord record{};
    record.magic = JOURNEY_MAGIC;
    record.version = JOURNEY_VERSION;
    record.completionReason = reason;
    record.hadConnectivityGap = state.hadConnectivityGap ? 1 : 0;
    strlcpy(record.journeyId, state.counterId, sizeof(record.journeyId));
    record.startedAtMs = state.startedAtMs;
    int64_t nowMs = 0;
    currentUtcMs(nowMs);
    record.endedAtMs = finalized
        ? (state.lastMovingAtMs >= state.startedAtMs ? state.lastMovingAtMs : nowMs)
        : 0;
    record.lastMovingAtMs = state.lastMovingAtMs;
    record.startOdometerKm = state.startOdometerKm;
    record.endOdometerKm = state.endOdometerKm;
    record.startSoc = state.startSoc;
    record.endSoc = state.endSoc;
    record.durationSeconds = finalized && record.endedAtMs >= state.startedAtMs
        ? static_cast<uint32_t>((record.endedAtMs - state.startedAtMs) / 1000) : 0;
    record.energyDrawnWh = max(0L, drawnWh());
    record.energyRegenWh = max(0L, regenWh());
    record.regenPeakW = state.regenPeakW;
    record.powerCoverageSeconds = state.powerCoverageMs / 1000;
    record.odometerAnomalyCount = state.odometerAnomalyCount;
    record.checksum = recordChecksum(record);
    return record;
}

void checkpointActive()
{
    if (!backfill.mounted || !c6Config.offlineCacheEnabled || !state.active || state.sealed ||
        !isfinite(state.startOdometerKm) || !isfinite(state.endOdometerKm) ||
        !isfinite(state.startSoc) || !isfinite(state.endSoc) || state.startedAtMs == 0) return;
    JourneyRecord record = snapshot(false);
    File file = LittleFS.open(ACTIVE_PATH, "w");
    if (!file) return;
    file.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record));
    file.flush();
    file.close();
    state.lastCheckpointMs = millis();
}

bool appendFinalized(uint8_t reason)
{
    if (!backfill.mounted || !c6Config.offlineCacheEnabled || !c6Config.motCloudEnabled ||
        !state.hadConnectivityGap ||
        backfill.pending >= JOURNEY_MAX_RECORDS) {
        if (state.hadConnectivityGap && backfill.pending >= JOURNEY_MAX_RECORDS) backfill.dropped++;
        return false;
    }
    JourneyRecord record = snapshot(true, reason);
    if (!validRecord(record)) return false;
    File file = LittleFS.open(JOURNEY_PATH, "a");
    if (!file) return false;
    const bool ok = file.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record)) == sizeof(record);
    file.flush();
    file.close();
    if (ok) backfill.pending++;
    return ok;
}

void scanJournal()
{
    backfill.pending = 0;
    File file = LittleFS.open(JOURNEY_PATH, "r");
    if (!file) return;
    JourneyRecord record{};
    while (file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record)) {
        if (!validRecord(record)) { backfill.corrupt++; break; }
        backfill.pending++;
    }
    file.close();
}

bool removeFirstRecord()
{
    File source = LittleFS.open(JOURNEY_PATH, "r");
    if (!source) return false;
    source.seek(sizeof(JourneyRecord));
    LittleFS.remove(JOURNEY_NEXT_PATH);
    File destination = LittleFS.open(JOURNEY_NEXT_PATH, "w");
    if (!destination) { source.close(); return false; }
    uint8_t buffer[128];
    while (source.available()) {
        const size_t count = source.read(buffer, sizeof(buffer));
        if (destination.write(buffer, count) != count) {
            destination.close(); source.close(); LittleFS.remove(JOURNEY_NEXT_PATH); return false;
        }
    }
    destination.flush(); destination.close(); source.close();
    LittleFS.remove(JOURNEY_PATH);
    if (!LittleFS.rename(JOURNEY_NEXT_PATH, JOURNEY_PATH)) return false;
    scanJournal();
    return true;
}

bool elapsedAtLeast(uint32_t nowMs, uint32_t sinceMs, uint32_t intervalMs)
{
    return sinceMs != 0 && static_cast<uint32_t>(nowMs - sinceMs) >= intervalMs;
}

void startJourney(uint32_t nowMs)
{
    state.active = true;
    state.sealed = false;
    state.publishPending = true;
    state.restartAfterPublish = false;
    state.journeySequence++;
    state.stoppedSinceMs = 0;
    state.lastIntegrationMs = 0;
    state.lastPublishMs = 0;
    state.drawnWattMs = 0.0;
    state.regenWattMs = 0.0;
    state.regenPeakW = 0;
    state.powerCoverageMs = 0;
    state.odometerAnomalyCount = 0;
    state.altitudeValid = false;
    state.altitudeAnchorValid = false;
    state.altitudeSampleCount = 0;
    state.altitudeMeters = NAN;
    state.altitudeAnchorMeters = NAN;
    state.altitudeAscentMeters = 0.0f;
    state.altitudeDescentMeters = 0.0f;
    state.altitudeAcceptedMs = 0;
    state.altitudeSampleMs = 0;
    state.hadConnectivityGap = !state.cloudConnected;
    state.lastCheckpointMs = 0;
    currentUtcMs(state.startedAtMs);
    state.startedAtUptimeMs = nowMs;
    state.lastMovingAtMs = state.startedAtMs;
    state.startOdometerKm = telemetry.display.odometerKm;
    state.endOdometerKm = telemetry.display.odometerKm;
    state.startSoc = telemetry.display.soc;
    state.endSoc = telemetry.display.soc;
    snprintf(
        state.counterId,
        sizeof(state.counterId),
        "%08lx-%lu",
        static_cast<unsigned long>(state.bootNonce),
        static_cast<unsigned long>(state.journeySequence));
    Serial.printf("Journey energy: started id=%s at=%lu ms\n",
                  state.counterId, static_cast<unsigned long>(nowMs));
}

long drawnWh()
{
    return static_cast<long>(floor(state.drawnWattMs / WATT_MS_PER_WH));
}

long regenWh()
{
    return static_cast<long>(floor(state.regenWattMs / WATT_MS_PER_WH));
}

void sealJourney(uint32_t nowMs, const char *reason)
{
    if (!state.active || state.sealed) return;
    state.sealed = true;
    state.publishPending = true;
    state.lastIntegrationMs = 0;
    appendFinalized(strcmp(reason, "charging") == 0 ? 1 : 2);
    LittleFS.remove(ACTIVE_PATH);
    Serial.printf(
        "Journey energy: sealed id=%s reason=%s at=%lu ms drawn=%ld Wh regen=%ld Wh\n",
        state.counterId, reason, static_cast<unsigned long>(nowMs),
        drawnWh(), regenWh());
}
}

void c6JourneyEnergySetup()
{
    state = JourneyEnergyState();
    backfill = BackfillState();
    backfill.mounted = LittleFS.begin(false);
    if (backfill.mounted) {
        LittleFS.mkdir("/offline-cache");
        if (LittleFS.exists(JOURNEY_NEXT_PATH) && !LittleFS.exists(JOURNEY_PATH))
            LittleFS.rename(JOURNEY_NEXT_PATH, JOURNEY_PATH);
        else if (LittleFS.exists(JOURNEY_NEXT_PATH)) LittleFS.remove(JOURNEY_NEXT_PATH);
        scanJournal();
        File active = LittleFS.open(ACTIVE_PATH, "r");
        JourneyRecord record{};
        if (active && active.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record)
            && validRecord(record, true)) {
            state.active = true;
            state.hadConnectivityGap = true;
            state.startedAtMs = record.startedAtMs;
            state.lastMovingAtMs = record.lastMovingAtMs;
            state.startOdometerKm = record.startOdometerKm;
            state.endOdometerKm = record.endOdometerKm;
            state.startSoc = record.startSoc;
            state.endSoc = record.endSoc;
            state.drawnWattMs = static_cast<double>(record.energyDrawnWh) * WATT_MS_PER_WH;
            state.regenWattMs = static_cast<double>(record.energyRegenWh) * WATT_MS_PER_WH;
            state.regenPeakW = record.regenPeakW;
            state.powerCoverageMs = record.powerCoverageSeconds * 1000UL;
            strlcpy(state.counterId, record.journeyId, sizeof(state.counterId));
            state.stoppedSinceMs = millis();
            if (state.stoppedSinceMs == 0) state.stoppedSinceMs = 1;
            Serial.printf("Journey energy: recovered active id=%s\n", state.counterId);
        }
        if (active) active.close();
    }
    state.bootNonce = esp_random();
    Serial.println("Journey energy: N16 durable counter enabled");
}

void c6JourneyEnergyLoop()
{
    const uint32_t nowMs = millis();
    const bool displayFresh = telemetry.display.valid &&
        static_cast<uint32_t>(nowMs - telemetry.display.lastUpdateMs) <= DISPLAY_FRESH_MS;
    const bool movingNow = displayFresh && isfinite(telemetry.display.speedKmh) &&
        telemetry.display.speedKmh > MOVING_SPEED_KMH;
    const bool freshChargeState = telemetry.bms.statusLastUpdateMs != 0 &&
        static_cast<uint32_t>(nowMs - telemetry.bms.statusLastUpdateMs) <= POWER_FRESH_MS;
    const bool chargingBoundary = freshChargeState &&
        (telemetry.bms.plugged || telemetryIsCharging());

    if (state.active && c6Config.motCloudEnabled && !state.cloudConnected)
        state.hadConnectivityGap = true;
    if (state.active && telemetry.display.valid) {
        if (isfinite(telemetry.display.odometerKm)) {
            if (!isfinite(state.endOdometerKm) || telemetry.display.odometerKm >= state.endOdometerKm)
                state.endOdometerKm = telemetry.display.odometerKm;
            else state.odometerAnomalyCount++;
        }
        if (isfinite(telemetry.display.soc)) state.endSoc = telemetry.display.soc;
    }
    if (state.active && movingNow) {
        int64_t movingAtMs = 0;
        if (currentUtcMs(movingAtMs)) state.lastMovingAtMs = movingAtMs;
    }

    if (state.active && !state.sealed && movingNow &&
        c6GpsAltitudePublishable(true)) {
        const float altitude = static_cast<float>(c6GpsAltitudeMeters());
        if (isfinite(altitude)) {
            state.altitudeValid = true;
            state.altitudeMeters = altitude;
            state.altitudeAcceptedMs = nowMs;
            if (!state.altitudeAnchorValid) {
                state.altitudeAnchorValid = true;
                state.altitudeAnchorMeters = altitude;
                state.altitudeSampleMs = nowMs;
                state.altitudeSampleCount = 1;
            } else if (static_cast<uint32_t>(nowMs - state.altitudeSampleMs) >=
                       ALTITUDE_SAMPLE_MS) {
                const float elapsedSeconds =
                    static_cast<float>(nowMs - state.altitudeSampleMs) / 1000.0f;
                const float delta = altitude - state.altitudeAnchorMeters;
                if (fabsf(delta) <= max(50.0f, elapsedSeconds * ALTITUDE_MAX_RATE_MPS)) {
                    if (state.altitudeSampleCount < UINT8_MAX)
                        state.altitudeSampleCount++;
                    if (fabsf(delta) >= ALTITUDE_DEADBAND_M) {
                        state.altitudeAnchorMeters = altitude;
                        if (delta > 0.0f) state.altitudeAscentMeters += delta;
                        else state.altitudeDescentMeters -= delta;
                    }
                }
                state.altitudeSampleMs = nowMs;
            }
        }
    }

    if (state.active && !state.sealed) {
        if (chargingBoundary) {
            sealJourney(nowMs, "charging");
        } else if (elapsedAtLeast(nowMs, state.stoppedSinceMs, JOURNEY_STOP_MS)) {
            sealJourney(nowMs, "10-minute-stop");
        }
    }

    if (movingNow && !chargingBoundary) {
        if (!state.active) {
            startJourney(nowMs);
        } else if (state.sealed) {
            if (state.publishPending) state.restartAfterPublish = true;
            else startJourney(nowMs);
        }
    }

    if (state.active && state.moving && !movingNow && state.stoppedSinceMs == 0) {
        state.stoppedSinceMs = nowMs;
        state.lastIntegrationMs = 0;
        Serial.printf("Journey energy: stop candidate id=%s at=%lu ms\n",
                      state.counterId, static_cast<unsigned long>(nowMs));
    } else if (state.active && !state.sealed && movingNow) {
        state.stoppedSinceMs = 0;
    }

    const bool freshPower = telemetry.bms.packCurrentValid &&
        isfinite(telemetry.bms.vehiclePowerW) &&
        static_cast<uint32_t>(nowMs - telemetry.bms.packCurrentLastUpdateMs) <= POWER_FRESH_MS;
    if (state.active && !state.sealed && movingNow && freshPower) {
        if (telemetry.bms.isRegenerating && telemetry.bms.vehiclePowerW < 0.0f) {
            const uint32_t candidate = static_cast<uint32_t>(lroundf(-telemetry.bms.vehiclePowerW));
            if (candidate > state.regenPeakW)
                state.regenPeakW = static_cast<uint16_t>(min(candidate, 65535UL));
        }
        if (state.lastIntegrationMs != 0) {
            const uint32_t stepMs = static_cast<uint32_t>(nowMs - state.lastIntegrationMs);
            if (stepMs <= MAX_INTEGRATION_STEP_MS) {
                const double wattMs = static_cast<double>(telemetry.bms.vehiclePowerW) * stepMs;
                if (wattMs >= 0.0) state.drawnWattMs += wattMs;
                else state.regenWattMs -= wattMs;
                state.powerCoverageMs += stepMs;
            }
        }
        state.lastIntegrationMs = nowMs;
    } else {
        state.lastIntegrationMs = 0;
    }

    state.moving = movingNow;
    if (state.active && !state.sealed &&
        (!state.lastCheckpointMs || elapsedAtLeast(nowMs, state.lastCheckpointMs, CHECKPOINT_MS)))
        checkpointActive();
}

bool c6JourneyEnergyAddToEnvelope(JsonObject values)
{
    if (!state.active || state.counterId[0] == '\0') return false;
    const uint32_t nowMs = millis();
    if (!state.publishPending && state.lastPublishMs != 0 &&
        !elapsedAtLeast(nowMs, state.lastPublishMs, CHECKPOINT_MS)) return false;
    values["journey/energy_counter_id"] = state.counterId;
    values["journey/energy_drawn_wh"] = drawnWh();
    values["journey/energy_regen_wh"] = regenWh();
    values["journey/regen_peak_w"] = state.regenPeakW;
    return true;
}

void c6JourneyEnergyEnvelopePublished()
{
    const uint32_t nowMs = millis();
    state.publishPending = false;
    state.lastPublishMs = nowMs;
    if (state.sealed && state.restartAfterPublish && state.moving) startJourney(nowMs);
}

void c6JourneyBackfillLoop(MotAwsIotClient &client, bool connected, bool freshLivePublished)
{
    state.cloudConnected = connected;
    if (!backfill.mounted || !c6Config.offlineCacheEnabled) return;
    if (!connected) {
        backfill.previousConnected = false;
        backfill.subscribed = false;
        backfill.waitingForAck = false;
        return;
    }
    if (!backfill.previousConnected) {
        backfill.previousConnected = true;
        backfill.subscribed = false;
    }
    if (!backfill.subscribed)
        backfill.subscribed = client.subscribe("journey/backfill/ack/v1", 1);
    if (!freshLivePublished || !backfill.subscribed || backfill.pending == 0 ||
        backfill.waitingForAck || backfill.blocked) return;
    const uint32_t nowMs = millis();
    if (backfill.lastAttemptMs && nowMs - backfill.lastAttemptMs < BACKFILL_RETRY_MS) return;
    backfill.lastAttemptMs = nowMs;
    File file = LittleFS.open(JOURNEY_PATH, "r");
    JourneyRecord record{};
    if (!file || file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) != sizeof(record) ||
        !validRecord(record)) {
        if (file) file.close();
        backfill.lastResult = "read_failed";
        return;
    }
    file.close();
    char id[65];
    snprintf(id, sizeof(id), "%s-%08lx", record.journeyId,
             static_cast<unsigned long>(record.checksum));
    JsonDocument doc;
    doc["version"] = 1;
    doc["vehicleId"] = client.credentials().vehicleId;
    doc["batchId"] = id;
    JsonObject item = doc["journeys"].to<JsonArray>().add<JsonObject>();
    item["journeyId"] = record.journeyId;
    item["startedAt"] = record.startedAtMs;
    item["endedAt"] = record.endedAtMs;
    item["completionReason"] = record.completionReason == 1 ? "charging" : "10-minute-stop";
    item["startOdometerKm"] = record.startOdometerKm;
    item["endOdometerKm"] = record.endOdometerKm;
    item["startSoc"] = record.startSoc;
    item["endSoc"] = record.endSoc;
    item["durationSeconds"] = record.durationSeconds;
    item["energyDrawnWh"] = record.energyDrawnWh;
    item["energyRegenWh"] = record.energyRegenWh;
    item["regenPeakW"] = record.regenPeakW;
    item["powerSampleCoverageSeconds"] = record.powerCoverageSeconds;
    item["odometerAnomalyCount"] = record.odometerAnomalyCount;
    item["firmwareVersion"] = MOT_VERSION;
    String payload;
    serializeJson(doc, payload);
    if (!client.publish("journey/backfill/v1", payload, false)) {
        backfill.lastResult = "publish_failed";
        return;
    }
    backfill.batchId = id;
    backfill.waitingForAck = true;
    backfill.lastResult = "awaiting_ack";
}

void c6JourneyBackfillHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length)
{
    if (!backfill.waitingForAck || !topic || !payload || length > 2048 ||
        !String(topic).endsWith("/journey/backfill/ack/v1")) return;
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) || doc["version"].as<int>() != 1 ||
        doc["batchId"].as<String>() != backfill.batchId) return;
    if (!doc["accepted"].as<bool>()) {
        backfill.rejected++;
        backfill.waitingForAck = false;
        backfill.blocked = doc["reason"].as<String>() != "disabled";
        backfill.lastResult = "rejected:" + doc["reason"].as<String>();
        return;
    }
    JsonArray journeys = doc["journeys"].as<JsonArray>();
    if (journeys.size() != 1) return;
    const String status = journeys[0]["status"].as<String>();
    if (status != "accepted" && status != "duplicate") {
        backfill.rejected++;
        backfill.blocked = true;
        backfill.waitingForAck = false;
        backfill.lastResult = "journey_rejected:" + status;
        return;
    }
    if (!removeFirstRecord()) {
        backfill.waitingForAck = false;
        backfill.lastResult = "ack_remove_failed";
        return;
    }
    backfill.replayed++;
    if (status == "duplicate") backfill.duplicates++;
    backfill.waitingForAck = false;
    backfill.blocked = false;
    backfill.batchId = "";
    backfill.lastAttemptMs = 0;
    backfill.lastResult = "acknowledged";
}

void c6JourneyBackfillPurge()
{
    if (!backfill.mounted) backfill.mounted = LittleFS.begin(false);
    if (backfill.mounted) {
        LittleFS.remove(JOURNEY_PATH);
        LittleFS.remove(JOURNEY_NEXT_PATH);
        LittleFS.remove(ACTIVE_PATH);
    }
    backfill.pending = 0;
    backfill.waitingForAck = false;
    backfill.batchId = "";
    backfill.lastResult = "purged";
}

String c6JourneyEnergyStatusJson()
{
    String out = "{\"enabled\":true,\"active\":";
    out += state.active ? "true" : "false";
    out += ",\"moving\":" + String(state.moving ? "true" : "false");
    out += ",\"sealed\":" + String(state.sealed ? "true" : "false");
    out += ",\"counterId\":\"" + String(state.counterId) + "\"";
    out += ",\"drawnWh\":" + String(drawnWh());
    out += ",\"regenWh\":" + String(regenWh());
    out += ",\"regenPeakW\":" + String(state.regenPeakW);
    out += ",\"journeyBackfill\":{\"pending\":" + String(backfill.pending);
    out += ",\"replayed\":" + String(backfill.replayed);
    out += ",\"duplicates\":" + String(backfill.duplicates);
    out += ",\"rejected\":" + String(backfill.rejected);
    out += ",\"dropped\":" + String(backfill.dropped);
    out += ",\"corrupt\":" + String(backfill.corrupt);
    out += ",\"waitingForAck\":" + String(backfill.waitingForAck ? "true" : "false");
    out += ",\"lastReplayResult\":\"" + backfill.lastResult + "\"}";
    out += "}";
    return out;
}

C6JourneyDisplaySnapshot c6JourneyDisplaySnapshot()
{
    C6JourneyDisplaySnapshot out;
    out.active = state.active && !state.sealed;
    if (!out.active) return out;
    if (isfinite(state.startOdometerKm) && isfinite(state.endOdometerKm) &&
        state.endOdometerKm >= state.startOdometerKm) {
        out.distanceKm = state.endOdometerKm - state.startOdometerKm;
        out.distanceValid = true;
        if (out.distanceKm >= 0.1f) {
            const float netWh = static_cast<float>(drawnWh() - regenWh());
            out.netKwhPer100Km = netWh / (out.distanceKm * 10.0f);
            out.consumptionValid = isfinite(out.netKwhPer100Km);
        }
    }
    if (state.startedAtUptimeMs != 0)
        out.durationSeconds = static_cast<uint32_t>((millis() - state.startedAtUptimeMs) / 1000U);
    else {
        int64_t nowMs = 0;
        if (currentUtcMs(nowMs) && state.startedAtMs > 0 && nowMs >= state.startedAtMs)
            out.durationSeconds = static_cast<uint32_t>((nowMs - state.startedAtMs) / 1000);
    }
    out.regenPeakW = state.regenPeakW;
    out.altitudeValid = state.altitudeValid && isfinite(state.altitudeMeters);
    out.elevationGainValid = out.altitudeValid && state.altitudeSampleCount >= 2;
    if (out.altitudeValid) {
        out.altitudeMeters = static_cast<int16_t>(constrain(
            lroundf(state.altitudeMeters), -32768L, 32767L));
        out.altitudeAgeMs = millis() - state.altitudeAcceptedMs;
    }
    out.ascentMeters = static_cast<uint16_t>(constrain(
        lroundf(state.altitudeAscentMeters), 0L, 65535L));
    out.descentMeters = static_cast<uint16_t>(constrain(
        lroundf(state.altitudeDescentMeters), 0L, 65535L));
    return out;
}

#else

void c6JourneyEnergySetup() {}
void c6JourneyEnergyLoop() {}
bool c6JourneyEnergyAddToEnvelope(JsonObject) { return false; }
void c6JourneyEnergyEnvelopePublished() {}
void c6JourneyBackfillLoop(MotAwsIotClient &, bool, bool) {}
void c6JourneyBackfillHandleAwsMessage(char *, uint8_t *, unsigned int) {}
void c6JourneyBackfillPurge() {}
String c6JourneyEnergyStatusJson() { return "{\"enabled\":false}"; }
C6JourneyDisplaySnapshot c6JourneyDisplaySnapshot() { return C6JourneyDisplaySnapshot(); }

#endif
