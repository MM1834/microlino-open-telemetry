#include "c6_charge_journal.h"

#include "c6_config.h"
#include "system/version.h"
#include "telemetry/telemetry.h"

#ifdef MOT_JOURNEY_ENERGY_COUNTER

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <MotAwsIot.h>
#include <esp_system.h>
#include <math.h>
#include <time.h>

namespace {
constexpr char PATH[] = "/offline-cache/charges.bin";
constexpr char NEXT_PATH[] = "/offline-cache/charges.next";
constexpr char LAST_DISPLAY_PATH[] = "/display-last-charge.bin";
constexpr char LAST_DISPLAY_NEXT_PATH[] = "/display-last-charge.next";
constexpr uint32_t MAGIC = 0x4D4F5443;
constexpr uint8_t VERSION = 1;
constexpr uint32_t LAST_DISPLAY_MAGIC = 0x4D4F5444;
constexpr uint8_t LAST_DISPLAY_VERSION = 1;
constexpr size_t MAX_RECORDS = 16;
constexpr uint32_t FRESH_MS = 3000;
constexpr uint32_t STOP_MS = 60000;
constexpr uint32_t MAX_STEP_MS = 2000;
constexpr uint32_t RETRY_MS = 30000;
constexpr time_t MIN_UTC = 1700000000;

struct ChargeRecord {
    uint32_t magic;
    uint8_t version;
    uint8_t reserved[3];
    char chargeId[40];
    int64_t startedAtMs;
    int64_t endedAtMs;
    float startOdometerKm;
    float endOdometerKm;
    float startSoc;
    float endSoc;
    uint32_t energyChargedWh;
    uint32_t powerCoverageMs;
    uint32_t checksum;
};

enum LastDisplayFlags : uint8_t {
    LAST_VALID = 1U << 0,
    LAST_SOC_VALID = 1U << 1,
    LAST_ENERGY_VALID = 1U << 2,
    LAST_DURATION_VALID = 1U << 3,
};

struct LastDisplayChargeRecord {
    uint32_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t sequence;
    uint8_t startSocPercent;
    uint8_t endSocPercent;
    uint16_t durationMinutes;
    uint32_t energyChargedWh;
    uint32_t checksum;
};

struct State {
    bool mounted = false;
    bool active = false;
    bool cloudConnected = false;
    bool hadGap = false;
    bool waiting = false;
    bool subscribed = false;
    uint32_t stopSince = 0;
    uint32_t lastIntegration = 0;
    uint32_t pending = 0;
    uint32_t sequence = 0;
    uint32_t nonce = 0;
    uint32_t lastAttempt = 0;
    double chargedWattMs = 0;
    uint32_t coveredMs = 0;
    int64_t startedAt = 0;
    float startOdo = NAN;
    float endOdo = NAN;
    float startSoc = NAN;
    float endSoc = NAN;
    String batchId;
    String result = "none";
    LastDisplayChargeRecord lastDisplayCharge{};
} state;

uint32_t checksum(const ChargeRecord &record) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < offsetof(ChargeRecord, checksum); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

uint32_t lastDisplayChecksum(const LastDisplayChargeRecord &record) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < offsetof(LastDisplayChargeRecord, checksum); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

bool utc(int64_t &value) {
    time_t now = time(nullptr);
    if (now < MIN_UTC) return false;
    value = static_cast<int64_t>(now) * 1000;
    return true;
}

bool valid(const ChargeRecord &r) {
    return r.magic == MAGIC && r.version == VERSION && r.chargeId[0] &&
        r.startedAtMs > 0 && r.endedAtMs >= r.startedAtMs &&
        isfinite(r.startOdometerKm) && isfinite(r.endOdometerKm) &&
        fabsf(r.endOdometerKm - r.startOdometerKm) <= 0.2f &&
        isfinite(r.startSoc) && isfinite(r.endSoc) && r.endSoc >= r.startSoc &&
        r.checksum == checksum(r);
}

bool validLastDisplayCharge(const LastDisplayChargeRecord &r) {
    return r.magic == LAST_DISPLAY_MAGIC && r.version == LAST_DISPLAY_VERSION &&
        (r.flags & LAST_VALID) &&
        (!(r.flags & LAST_SOC_VALID) ||
         (r.startSocPercent <= 100 && r.endSocPercent <= 100)) &&
        r.checksum == lastDisplayChecksum(r);
}

void saveLastDisplayCharge() {
    int64_t ended = 0;
    if (!utc(ended)) return;
    LastDisplayChargeRecord record{};
    record.magic = LAST_DISPLAY_MAGIC;
    record.version = LAST_DISPLAY_VERSION;
    record.flags = LAST_VALID;
    record.sequence = state.lastDisplayCharge.sequence == UINT16_MAX
        ? 1U : static_cast<uint16_t>(state.lastDisplayCharge.sequence + 1U);
    if (isfinite(state.startSoc) && isfinite(state.endSoc) &&
        state.startSoc >= 0.0F && state.startSoc <= 100.0F &&
        state.endSoc >= 0.0F && state.endSoc <= 100.0F) {
        record.flags |= LAST_SOC_VALID;
        record.startSocPercent = static_cast<uint8_t>(lroundf(state.startSoc));
        record.endSocPercent = static_cast<uint8_t>(lroundf(state.endSoc));
    }
    if (state.coveredMs > 0) {
        record.flags |= LAST_ENERGY_VALID;
        record.energyChargedWh = static_cast<uint32_t>(max(
            0.0, state.chargedWattMs / 3600000.0));
    }
    if (state.startedAt > 0 && ended >= state.startedAt) {
        record.flags |= LAST_DURATION_VALID;
        const uint64_t minutes = static_cast<uint64_t>(ended - state.startedAt) / 60000ULL;
        record.durationMinutes = static_cast<uint16_t>(min<uint64_t>(minutes, UINT16_MAX));
    }
    record.checksum = lastDisplayChecksum(record);
    File file = LittleFS.open(LAST_DISPLAY_NEXT_PATH, "w");
    if (!file) return;
    const bool written = file.write(reinterpret_cast<const uint8_t *>(&record),
                                    sizeof(record)) == sizeof(record);
    file.flush();
    file.close();
    if (!written) {
        LittleFS.remove(LAST_DISPLAY_NEXT_PATH);
        return;
    }
    LittleFS.remove(LAST_DISPLAY_PATH);
    if (LittleFS.rename(LAST_DISPLAY_NEXT_PATH, LAST_DISPLAY_PATH))
        state.lastDisplayCharge = record;
}

void startCharge() {
    int64_t now = 0;
    if (!utc(now) || !telemetry.display.valid || !isfinite(telemetry.display.soc) ||
        !isfinite(telemetry.display.odometerKm)) return;
    state.active = true;
    state.hadGap = !state.cloudConnected;
    state.stopSince = 0;
    state.lastIntegration = 0;
    state.chargedWattMs = 0;
    state.coveredMs = 0;
    state.startedAt = now;
    state.startOdo = state.endOdo = telemetry.display.odometerKm;
    state.startSoc = state.endSoc = telemetry.display.soc;
}

void appendCharge() {
    int64_t ended = 0;
    if (!state.hadGap || !utc(ended) || state.pending >= MAX_RECORDS) return;
    ChargeRecord r{};
    r.magic = MAGIC; r.version = VERSION;
    snprintf(r.chargeId, sizeof(r.chargeId), "%08lx-%08lx-%lu",
             static_cast<unsigned long>(state.nonce),
             static_cast<unsigned long>(state.startedAt / 1000),
             static_cast<unsigned long>(++state.sequence));
    r.startedAtMs = state.startedAt; r.endedAtMs = ended;
    r.startOdometerKm = state.startOdo; r.endOdometerKm = state.endOdo;
    r.startSoc = state.startSoc; r.endSoc = state.endSoc;
    r.energyChargedWh = static_cast<uint32_t>(max(0.0, state.chargedWattMs / 3600000.0));
    r.powerCoverageMs = state.coveredMs; r.checksum = checksum(r);
    if (!valid(r)) return;
    File f = LittleFS.open(PATH, "a");
    if (!f) return;
    if (f.write(reinterpret_cast<uint8_t *>(&r), sizeof(r)) == sizeof(r)) state.pending++;
    f.flush(); f.close();
}

bool removeFirst() {
    File in = LittleFS.open(PATH, "r");
    if (!in) return false;
    ChargeRecord skipped{};
    if (in.read(reinterpret_cast<uint8_t *>(&skipped), sizeof(skipped)) != sizeof(skipped)) { in.close(); return false; }
    File out = LittleFS.open(NEXT_PATH, "w");
    ChargeRecord item{};
    while (out && in.read(reinterpret_cast<uint8_t *>(&item), sizeof(item)) == sizeof(item))
        out.write(reinterpret_cast<uint8_t *>(&item), sizeof(item));
    in.close(); if (!out) return false; out.flush(); out.close();
    LittleFS.remove(PATH); LittleFS.rename(NEXT_PATH, PATH);
    if (state.pending) state.pending--;
    return true;
}
}

void c6ChargeJournalSetup() {
    state = State(); state.nonce = esp_random(); state.mounted = LittleFS.begin(false);
    if (!state.mounted) return;
    File last = LittleFS.open(LAST_DISPLAY_PATH, "r");
    LastDisplayChargeRecord lastRecord{};
    if (last && last.read(reinterpret_cast<uint8_t *>(&lastRecord), sizeof(lastRecord)) ==
                    sizeof(lastRecord) && validLastDisplayCharge(lastRecord))
        state.lastDisplayCharge = lastRecord;
    if (last) last.close();
    File f = LittleFS.open(PATH, "r"); ChargeRecord r{};
    while (f && f.read(reinterpret_cast<uint8_t *>(&r), sizeof(r)) == sizeof(r)) {
        if (!valid(r)) break; state.pending++;
    }
    if (f) f.close();
}

void c6ChargeJournalLoop() {
    const uint32_t now = millis();
    const bool freshDisplay = telemetry.display.valid && now - telemetry.display.lastUpdateMs <= FRESH_MS;
    const bool freshCharge = (telemetry.bms.statusLastUpdateMs && now - telemetry.bms.statusLastUpdateMs <= FRESH_MS) ||
        (telemetry.charging.valid && now - telemetry.charging.lastUpdateMs <= FRESH_MS);
    const bool charging = freshCharge && telemetryIsCharging();
    if (charging && !state.active) startCharge();
    if (!state.active) return;
    if (!state.cloudConnected) state.hadGap = true;
    if (freshDisplay) {
        state.endOdo = telemetry.display.odometerKm;
        state.endSoc = telemetry.display.soc;
    }
    if (charging) state.stopSince = 0;
    else if (!state.stopSince) state.stopSince = now;
    float watts = NAN;
    if (telemetry.bms.packCurrentValid && now - telemetry.bms.packCurrentLastUpdateMs <= FRESH_MS)
        watts = telemetry.bms.vehiclePowerW;
    else if (telemetry.charging.valid) watts = telemetry.charging.powerSigned * 100.0f;
    if (charging && isfinite(watts) && watts < 0) {
        if (state.lastIntegration) {
            uint32_t step = now - state.lastIntegration;
            if (step <= MAX_STEP_MS) { state.chargedWattMs += -watts * step; state.coveredMs += step; }
        }
        state.lastIntegration = now;
    } else state.lastIntegration = 0;
    if (state.stopSince && now - state.stopSince >= STOP_MS) {
        saveLastDisplayCharge(); appendCharge(); state.active = false; state.stopSince = 0;
    }
}

void c6ChargeBackfillLoop(MotAwsIotClient &client, bool connected, bool freshLivePublished) {
    state.cloudConnected = connected;
    if (!state.mounted || !c6Config.offlineCacheEnabled || !connected) {
        state.subscribed = false; state.waiting = false; return;
    }
    if (!state.subscribed) state.subscribed = client.subscribe("charge/backfill/ack/v1", 1);
    if (!freshLivePublished || !state.subscribed || !state.pending || state.waiting) return;
    uint32_t now = millis(); if (state.lastAttempt && now - state.lastAttempt < RETRY_MS) return;
    state.lastAttempt = now;
    File f = LittleFS.open(PATH, "r"); ChargeRecord r{};
    if (!f || f.read(reinterpret_cast<uint8_t *>(&r), sizeof(r)) != sizeof(r) || !valid(r)) {
        if (f) f.close(); state.result = "read_failed"; return;
    }
    f.close();
    char batch[64]; snprintf(batch, sizeof(batch), "%s-%08lx", r.chargeId, static_cast<unsigned long>(r.checksum));
    JsonDocument doc; doc["version"] = 1; doc["vehicleId"] = client.credentials().vehicleId; doc["batchId"] = batch;
    JsonObject c = doc["charge"].to<JsonObject>();
    c["chargeId"] = r.chargeId; c["startedAt"] = r.startedAtMs; c["endedAt"] = r.endedAtMs;
    c["startOdometerKm"] = r.startOdometerKm; c["endOdometerKm"] = r.endOdometerKm;
    c["startSoc"] = r.startSoc; c["endSoc"] = r.endSoc;
    c["energyChargedWh"] = r.energyChargedWh; c["powerCoverageMs"] = r.powerCoverageMs;
    c["firmwareVersion"] = MOT_VERSION;
    String payload; serializeJson(doc, payload);
    if (!client.publish("charge/backfill/v1", payload, false)) { state.result = "publish_failed"; return; }
    state.batchId = batch; state.waiting = true; state.result = "awaiting_ack";
}

void c6ChargeBackfillHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length) {
    if (!state.waiting || !topic || !payload || length > 1024 ||
        !String(topic).endsWith("/charge/backfill/ack/v1")) return;
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) || doc["version"].as<int>() != 1 ||
        doc["batchId"].as<String>() != state.batchId) return;
    String status = doc["status"].as<String>();
    if (!doc["accepted"].as<bool>() || (status != "accepted" && status != "duplicate")) {
        state.waiting = false; state.result = "rejected"; return;
    }
    if (!removeFirst()) { state.waiting = false; state.result = "remove_failed"; return; }
    state.waiting = false; state.batchId = ""; state.lastAttempt = 0; state.result = "acknowledged";
}

void c6ChargeJournalPurge() {
    if (!state.mounted) state.mounted = LittleFS.begin(false);
    if (state.mounted) { LittleFS.remove(PATH); LittleFS.remove(NEXT_PATH); }
    state.pending = 0; state.waiting = false; state.result = "purged";
}

String c6ChargeJournalStatusJson() {
    return String("{\"enabled\":true,\"active\":") + (state.active ? "true" : "false") +
        ",\"pending\":" + String(state.pending) + ",\"lastReplayResult\":\"" + state.result + "\"}";
}

C6ChargeDisplaySnapshot c6ChargeDisplaySnapshot() {
    C6ChargeDisplaySnapshot out;
    out.active = state.active;
    out.energyValid = state.active && state.coveredMs > 0;
    out.energyChargedWh = static_cast<uint32_t>(max(0.0, state.chargedWattMs / 3600000.0));
    const LastDisplayChargeRecord &last = state.lastDisplayCharge;
    out.completedValid = validLastDisplayCharge(last);
    out.completedSocValid = out.completedValid && (last.flags & LAST_SOC_VALID);
    out.completedEnergyValid = out.completedValid && (last.flags & LAST_ENERGY_VALID);
    out.completedDurationValid = out.completedValid && (last.flags & LAST_DURATION_VALID);
    out.completedStartSocPercent = last.startSocPercent;
    out.completedEndSocPercent = last.endSocPercent;
    out.completedDurationMinutes = last.durationMinutes;
    out.completedEnergyChargedWh = last.energyChargedWh;
    out.completedSequence = last.sequence;
    return out;
}

#else
void c6ChargeJournalSetup() {}
void c6ChargeJournalLoop() {}
void c6ChargeBackfillLoop(MotAwsIotClient &, bool, bool) {}
void c6ChargeBackfillHandleAwsMessage(char *, uint8_t *, unsigned int) {}
void c6ChargeJournalPurge() {}
String c6ChargeJournalStatusJson() { return "{\"enabled\":false}"; }
C6ChargeDisplaySnapshot c6ChargeDisplaySnapshot() { return C6ChargeDisplaySnapshot(); }
#endif
