#include "lilygo_offline_cache.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <MotAwsIot.h>
#include <time.h>

#include "config/lilygo_config.h"
#include "telemetry/telemetry.h"

namespace {
#ifndef MOT_OFFLINE_CACHE_MAX_BYTES
#define MOT_OFFLINE_CACHE_MAX_BYTES 131072
#endif
constexpr size_t MAX_BYTES = MOT_OFFLINE_CACHE_MAX_BYTES;
constexpr char DIRECTORY[] = "/offline-cache";
constexpr char PATH[] = "/offline-cache/history.bin";
constexpr char NEXT_PATH[] = "/offline-cache/history.next";
constexpr uint32_t MAGIC = 0x4D4F5443;
constexpr uint8_t VERSION = 1;
constexpr time_t MIN_VALID_UTC = 1700000000;
constexpr uint32_t SOC_INTERVAL_SECONDS = 300;
constexpr uint32_t SPEED_INTERVAL_SECONDS = 60;
constexpr uint32_t REPLAY_RETRY_MS = 30000;
constexpr size_t BATCH_RECORDS = 8;

enum class Signal : uint8_t { Soc = 1, Speed = 2 };

struct Record {
    uint32_t magic;
    uint8_t version;
    uint8_t signal;
    uint16_t reserved;
    int64_t sampledAtMs;
    float value;
    uint32_t checksum;
};
static_assert(sizeof(Record) <= 32, "offline-cache record must remain compact");

struct State {
    bool mounted = false;
    bool subscribed = false;
    bool previousConnected = false;
    bool waitingForAck = false;
    bool replayBlocked = false;
    bool speedWasActive = false;
    uint32_t recordCount = 0;
    uint32_t droppedCount = 0;
    uint32_t corruptCount = 0;
    uint32_t replayedCount = 0;
    uint32_t duplicateCount = 0;
    uint32_t rejectedCount = 0;
    uint32_t lastSocBucket = 0;
    uint32_t lastSpeedBucket = 0;
    uint32_t lastReplayAttemptMs = 0;
    uint32_t pendingBatchCount = 0;
    uint64_t oldestSampleMs = 0;
    String pendingBatchId;
    String lastReplayResult = "none";
} state;

uint32_t checksum(const Record &record)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < offsetof(Record, checksum); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

bool valid(const Record &record)
{
    return record.magic == MAGIC && record.version == VERSION &&
        (record.signal == static_cast<uint8_t>(Signal::Soc) ||
         record.signal == static_cast<uint8_t>(Signal::Speed)) &&
        record.sampledAtMs >= static_cast<int64_t>(MIN_VALID_UTC) * 1000 &&
        isfinite(record.value) && record.checksum == checksum(record);
}

void recoverJournal()
{
    if (!LittleFS.exists(NEXT_PATH)) return;
    if (LittleFS.exists(PATH)) LittleFS.remove(NEXT_PATH);
    else LittleFS.rename(NEXT_PATH, PATH);
}

bool rewriteAfter(size_t skippedRecords)
{
    File source = LittleFS.open(PATH, "r");
    if (!source) return false;
    LittleFS.remove(NEXT_PATH);
    File destination = LittleFS.open(NEXT_PATH, "w");
    if (!destination) { source.close(); return false; }
    Record record{};
    size_t index = 0;
    bool ok = true;
    while (source.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record)) {
        if (index++ < skippedRecords) continue;
        if (!valid(record)) break;
        if (destination.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record)) != sizeof(record)) {
            ok = false;
            break;
        }
    }
    destination.flush();
    destination.close();
    source.close();
    if (!ok) { LittleFS.remove(NEXT_PATH); return false; }
    return LittleFS.remove(PATH) && LittleFS.rename(NEXT_PATH, PATH);
}

void scanJournal()
{
    state.recordCount = 0;
    state.oldestSampleMs = 0;
    File file = LittleFS.open(PATH, "r");
    if (!file) return;
    Record record{};
    while (file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record)) {
        if (!valid(record)) { state.corruptCount++; break; }
        if (!state.oldestSampleMs) state.oldestSampleMs = record.sampledAtMs;
        state.recordCount++;
    }
    const bool damaged = file.size() != state.recordCount * sizeof(Record);
    file.close();
    if (damaged) rewriteAfter(0);
}

bool append(Signal signal, float value, int64_t sampledAtMs)
{
    if (!state.mounted || !config.offlineCacheEnabled) return false;
    if ((state.recordCount + 1) * sizeof(Record) > MAX_BYTES) {
        state.droppedCount++;
        return false;
    }
    Record record{MAGIC, VERSION, static_cast<uint8_t>(signal), 0, sampledAtMs, value, 0};
    record.checksum = checksum(record);
    File file = LittleFS.open(PATH, "a");
    if (!file) { state.droppedCount++; return false; }
    const bool stored = file.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record)) == sizeof(record);
    file.flush();
    file.close();
    if (!stored) { state.droppedCount++; return false; }
    if (!state.oldestSampleMs) state.oldestSampleMs = sampledAtMs;
    state.recordCount++;
    return true;
}

bool chargingActive()
{
    return telemetryIsCharging() ||
        (telemetry.bms.packStatusValid && telemetry.bms.plugged) ||
        (telemetry.charging.valid && telemetry.charging.plugged);
}

void sampleOffline()
{
    if (!config.offlineCacheEnabled || !telemetry.display.valid) return;
    const time_t now = time(nullptr);
    if (now < MIN_VALID_UTC) return;
    const int64_t sampledAtMs = static_cast<int64_t>(now) * 1000;
    const uint32_t nowSeconds = static_cast<uint32_t>(now);
    const bool moving = telemetry.display.speedKmh > 1.0f;
    const uint32_t speedBucket = nowSeconds - nowSeconds % SPEED_INTERVAL_SECONDS;
    if (moving && speedBucket != state.lastSpeedBucket) {
        if (append(Signal::Speed, telemetry.display.speedKmh, sampledAtMs)) state.lastSpeedBucket = speedBucket;
    } else if (!moving && state.speedWasActive) {
        append(Signal::Speed, 0.0f, sampledAtMs);
    }
    state.speedWasActive = moving;
    const uint32_t socBucket = nowSeconds - nowSeconds % SOC_INTERVAL_SECONDS;
    if ((moving || chargingActive()) && socBucket != state.lastSocBucket) {
        if (append(Signal::Soc, telemetry.display.soc, sampledAtMs)) state.lastSocBucket = socBucket;
    }
}

bool readBatch(Record *records, size_t &count)
{
    count = 0;
    File file = LittleFS.open(PATH, "r");
    if (!file) return false;
    while (count < BATCH_RECORDS &&
        file.read(reinterpret_cast<uint8_t *>(&records[count]), sizeof(Record)) == sizeof(Record) &&
        valid(records[count])) count++;
    file.close();
    return count > 0;
}

bool publishBatch(MotAwsIotClient &client)
{
    Record records[BATCH_RECORDS]{};
    size_t count = 0;
    if (!readBatch(records, count)) return false;
    char id[65];
    snprintf(id, sizeof(id), "%llx-%lu-%08lx",
        static_cast<unsigned long long>(records[0].sampledAtMs),
        static_cast<unsigned long>(count),
        static_cast<unsigned long>(records[count - 1].checksum));
    JsonDocument doc;
    doc["version"] = 1;
    doc["vehicleId"] = client.credentials().vehicleId;
    doc["batchId"] = id;
    JsonArray samples = doc["samples"].to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
        JsonObject sample = samples.add<JsonObject>();
        sample["signal"] = records[i].signal == static_cast<uint8_t>(Signal::Soc) ? "soc" : "speed";
        sample["sampledAt"] = records[i].sampledAtMs;
        sample["value"] = records[i].value;
    }
    String payload;
    serializeJson(doc, payload);
    if (!client.publish("history/backfill/v1", payload, false)) {
        state.lastReplayResult = "publish_failed";
        return false;
    }
    state.pendingBatchId = id;
    state.pendingBatchCount = count;
    state.waitingForAck = true;
    state.lastReplayResult = "awaiting_ack";
    return true;
}
}

void lilygoOfflineCacheSetup()
{
    state = State();
    state.mounted = LittleFS.begin(false);
    if (!state.mounted) return;
    LittleFS.mkdir(DIRECTORY);
    recoverJournal();
    scanJournal();
}

void lilygoOfflineCacheLoop(MotAwsIotClient &client, bool awsConnected, bool freshLivePublished)
{
    if (!state.mounted || !config.offlineCacheEnabled) return;
    if (!awsConnected) {
        state.subscribed = false;
        state.waitingForAck = false;
        state.previousConnected = false;
        sampleOffline();
        return;
    }
    if (!state.previousConnected) { state.previousConnected = true; state.subscribed = false; }
    if (!state.subscribed) state.subscribed = client.subscribe("history/backfill/ack/v1", 1);
    if (!freshLivePublished || !state.subscribed || state.recordCount == 0 ||
        state.waitingForAck || state.replayBlocked) return;
    if (state.lastReplayAttemptMs && millis() - state.lastReplayAttemptMs < REPLAY_RETRY_MS) return;
    state.lastReplayAttemptMs = millis();
    publishBatch(client);
}

void lilygoOfflineCacheHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length)
{
    if (!state.waitingForAck || !topic || !payload || length > 1024 ||
        !String(topic).endsWith("/history/backfill/ack/v1")) return;
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) || doc["version"].as<int>() != 1 ||
        doc["batchId"].as<String>() != state.pendingBatchId) return;
    if (!doc["accepted"].as<bool>()) {
        state.rejectedCount++;
        state.replayBlocked = true;
        state.lastReplayResult = "rejected:" + doc["reason"].as<String>();
        state.waitingForAck = false;
        return;
    }
    if (doc["sampleCount"].as<uint32_t>() != state.pendingBatchCount) return;
    if (!rewriteAfter(state.pendingBatchCount)) {
        state.lastReplayResult = "ack_remove_failed";
        state.waitingForAck = false;
        return;
    }
    state.replayedCount += state.pendingBatchCount;
    state.duplicateCount += doc["duplicates"].as<uint32_t>();
    state.pendingBatchCount = 0;
    state.pendingBatchId = "";
    state.waitingForAck = false;
    state.replayBlocked = false;
    state.lastReplayResult = "acknowledged";
    state.lastReplayAttemptMs = 0;
    scanJournal();
}

void lilygoOfflineCachePurge()
{
    if (!state.mounted) state.mounted = LittleFS.begin(false);
    if (state.mounted) { LittleFS.remove(PATH); LittleFS.remove(NEXT_PATH); }
    state.recordCount = 0;
    state.oldestSampleMs = 0;
    state.waitingForAck = false;
    state.pendingBatchCount = 0;
    state.pendingBatchId = "";
    state.lastReplayResult = "purged";
}

String lilygoOfflineCacheStatusJson()
{
    const uint64_t nowMs = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
    const uint64_t age = state.oldestSampleMs && nowMs >= state.oldestSampleMs
        ? (nowMs - state.oldestSampleMs) / 1000ULL : 0;
    String out = "{\"enabled\":" + String(config.offlineCacheEnabled ? "true" : "false");
    out += ",\"mounted\":" + String(state.mounted ? "true" : "false");
    out += ",\"maxBytes\":" + String(static_cast<unsigned long>(MAX_BYTES));
    out += ",\"usedBytes\":" + String(static_cast<unsigned long>(state.recordCount * sizeof(Record)));
    out += ",\"pending\":" + String(state.recordCount);
    out += ",\"oldestAgeSec\":" + String(static_cast<unsigned long long>(age));
    out += ",\"dropped\":" + String(state.droppedCount);
    out += ",\"corrupt\":" + String(state.corruptCount);
    out += ",\"replayed\":" + String(state.replayedCount);
    out += ",\"duplicates\":" + String(state.duplicateCount);
    out += ",\"rejected\":" + String(state.rejectedCount);
    out += ",\"waitingForAck\":" + String(state.waitingForAck ? "true" : "false");
    out += ",\"replayBlocked\":" + String(state.replayBlocked ? "true" : "false");
    out += ",\"lastReplayResult\":\"" + state.lastReplayResult + "\"}";
    return out;
}
