#include "c6_offline_cache.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <MotAwsIot.h>
#include <time.h>

#include "c6_config.h"
#include "telemetry/telemetry.h"

#ifndef MOT_OFFLINE_CACHE_MAX_BYTES
#define MOT_OFFLINE_CACHE_MAX_BYTES 131072
#endif

namespace {
constexpr char CACHE_DIRECTORY[] = "/offline-cache";
constexpr char CACHE_PATH[] = "/offline-cache/history.bin";
constexpr char NEXT_PATH[] = "/offline-cache/history.next";
constexpr uint32_t RECORD_MAGIC = 0x4D4F5443;
constexpr uint8_t RECORD_VERSION = 1;
constexpr time_t MIN_VALID_UTC = 1700000000;
constexpr uint32_t SOC_INTERVAL_SECONDS = 300;
constexpr uint32_t SPEED_INTERVAL_SECONDS = 60;
constexpr uint32_t REPLAY_RETRY_MS = 30000;
constexpr size_t REPLAY_BATCH_RECORDS = 8;

enum class CachedSignal : uint8_t { Soc = 1, Speed = 2 };

struct CacheRecord {
    uint32_t magic;
    uint8_t version;
    uint8_t signal;
    uint16_t reserved;
    int64_t sampledAtMs;
    float value;
    uint32_t checksum;
};

static_assert(sizeof(CacheRecord) <= 32, "CACHE-001 record must remain compact");

struct CacheState {
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
};

CacheState state;

uint32_t checksum(const CacheRecord &record)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < offsetof(CacheRecord, checksum); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

bool validRecord(const CacheRecord &record)
{
    return record.magic == RECORD_MAGIC &&
           record.version == RECORD_VERSION &&
           (record.signal == static_cast<uint8_t>(CachedSignal::Soc) ||
            record.signal == static_cast<uint8_t>(CachedSignal::Speed)) &&
           record.sampledAtMs >= static_cast<int64_t>(MIN_VALID_UTC) * 1000 &&
           isfinite(record.value) && record.checksum == checksum(record);
}

void recoverJournal()
{
    if (LittleFS.exists(NEXT_PATH)) {
        if (LittleFS.exists(CACHE_PATH)) {
            LittleFS.remove(NEXT_PATH);
        } else {
            LittleFS.rename(NEXT_PATH, CACHE_PATH);
        }
    }
}

bool keepJournalPrefix(size_t validBytes)
{
    File source = LittleFS.open(CACHE_PATH, "r");
    if (!source) return false;
    LittleFS.remove(NEXT_PATH);
    File destination = LittleFS.open(NEXT_PATH, "w");
    if (!destination) { source.close(); return false; }
    uint8_t buffer[64];
    size_t remaining = validBytes;
    bool ok = true;
    while (remaining) {
        const size_t requested = min(remaining, sizeof(buffer));
        const size_t read = source.read(buffer, requested);
        if (read != requested || destination.write(buffer, read) != read) {
            ok = false;
            break;
        }
        remaining -= read;
    }
    destination.flush();
    destination.close();
    source.close();
    if (!ok) { LittleFS.remove(NEXT_PATH); return false; }
    if (!LittleFS.remove(CACHE_PATH) || !LittleFS.rename(NEXT_PATH, CACHE_PATH)) return false;
    return true;
}

void scanJournal()
{
    state.recordCount = 0;
    state.oldestSampleMs = 0;
    File file = LittleFS.open(CACHE_PATH, "r+");
    if (!file) return;
    size_t validBytes = 0;
    CacheRecord record{};
    while (file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record)) {
        if (!validRecord(record)) {
            state.corruptCount++;
            break;
        }
        if (!state.oldestSampleMs) state.oldestSampleMs = record.sampledAtMs;
        state.recordCount++;
        validBytes += sizeof(record);
    }
    const bool needsRepair = file.size() != validBytes;
    file.close();
    if (needsRepair) keepJournalPrefix(validBytes);
}

bool append(CachedSignal signal, float value, int64_t sampledAtMs)
{
    if (!state.mounted || !c6Config.offlineCacheEnabled) return false;
    const size_t used = static_cast<size_t>(state.recordCount) * sizeof(CacheRecord);
    if (used + sizeof(CacheRecord) > MOT_OFFLINE_CACHE_MAX_BYTES) {
        state.droppedCount++;
        return false;
    }
    CacheRecord record{};
    record.magic = RECORD_MAGIC;
    record.version = RECORD_VERSION;
    record.signal = static_cast<uint8_t>(signal);
    record.sampledAtMs = sampledAtMs;
    record.value = value;
    record.checksum = checksum(record);
    File file = LittleFS.open(CACHE_PATH, "a");
    if (!file) {
        state.droppedCount++;
        return false;
    }
    const bool stored = file.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record)) == sizeof(record);
    file.flush();
    file.close();
    if (!stored) {
        state.droppedCount++;
        return false;
    }
    if (!state.oldestSampleMs) state.oldestSampleMs = sampledAtMs;
    state.recordCount++;
    return true;
}

bool currentUtcMs(int64_t &value)
{
    const time_t now = time(nullptr);
    if (now < MIN_VALID_UTC) return false;
    value = static_cast<int64_t>(now) * 1000;
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
    if (!c6Config.offlineCacheEnabled || !telemetry.display.valid) return;
    int64_t sampledAtMs = 0;
    if (!currentUtcMs(sampledAtMs)) return;
    const uint32_t nowSeconds = static_cast<uint32_t>(sampledAtMs / 1000);
    const bool moving = telemetry.display.speedKmh > 1.0f;

    const uint32_t speedBucket = nowSeconds - nowSeconds % SPEED_INTERVAL_SECONDS;
    if (moving && speedBucket != state.lastSpeedBucket) {
        if (append(CachedSignal::Speed, telemetry.display.speedKmh, sampledAtMs)) {
            state.lastSpeedBucket = speedBucket;
        }
    } else if (!moving && state.speedWasActive) {
        append(CachedSignal::Speed, 0.0f, sampledAtMs);
    }
    state.speedWasActive = moving;

    const uint32_t socBucket = nowSeconds - nowSeconds % SOC_INTERVAL_SECONDS;
    if ((moving || chargingActive()) && socBucket != state.lastSocBucket) {
        if (append(CachedSignal::Soc, telemetry.display.soc, sampledAtMs)) {
            state.lastSocBucket = socBucket;
        }
    }
}

bool readBatch(CacheRecord *records, size_t &count)
{
    count = 0;
    File file = LittleFS.open(CACHE_PATH, "r");
    if (!file) return false;
    while (count < REPLAY_BATCH_RECORDS &&
           file.read(reinterpret_cast<uint8_t *>(&records[count]), sizeof(CacheRecord)) == sizeof(CacheRecord)) {
        if (!validRecord(records[count])) break;
        count++;
    }
    file.close();
    return count > 0;
}

String batchId(const CacheRecord *records, size_t count)
{
    char value[65];
    snprintf(value, sizeof(value), "%llx-%lu-%08lx",
             static_cast<unsigned long long>(records[0].sampledAtMs),
             static_cast<unsigned long>(count),
             static_cast<unsigned long>(records[count - 1].checksum));
    return value;
}

bool publishBatch(MotAwsIotClient &client)
{
    CacheRecord records[REPLAY_BATCH_RECORDS]{};
    size_t count = 0;
    if (!readBatch(records, count)) return false;

    JsonDocument doc;
    doc["version"] = 1;
    doc["vehicleId"] = client.credentials().vehicleId;
    const String id = batchId(records, count);
    doc["batchId"] = id;
    JsonArray samples = doc["samples"].to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
        JsonObject sample = samples.add<JsonObject>();
        sample["signal"] = records[i].signal == static_cast<uint8_t>(CachedSignal::Soc)
            ? "soc" : "speed";
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

bool removeAcknowledged(size_t count)
{
    File source = LittleFS.open(CACHE_PATH, "r");
    if (!source) return false;
    LittleFS.remove(NEXT_PATH);
    File destination = LittleFS.open(NEXT_PATH, "w");
    if (!destination) { source.close(); return false; }
    CacheRecord record{};
    size_t index = 0;
    bool ok = true;
    while (source.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record)) {
        if (index++ < count) continue;
        if (destination.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record)) != sizeof(record)) {
            ok = false;
            break;
        }
    }
    destination.flush();
    destination.close();
    source.close();
    if (!ok) { LittleFS.remove(NEXT_PATH); return false; }
    if (!LittleFS.remove(CACHE_PATH) || !LittleFS.rename(NEXT_PATH, CACHE_PATH)) return false;
    scanJournal();
    return true;
}
}

void c6OfflineCacheSetup()
{
    state = CacheState();
    state.mounted = LittleFS.begin(false);
    if (!state.mounted) return;
    LittleFS.mkdir(CACHE_DIRECTORY);
    recoverJournal();
    scanJournal();
}

void c6OfflineCacheLoop(MotAwsIotClient &client, bool awsConnected, bool freshLivePublished)
{
    if (!state.mounted || !c6Config.offlineCacheEnabled) return;
    if (!awsConnected) {
        state.subscribed = false;
        state.waitingForAck = false;
        state.previousConnected = false;
        sampleOffline();
        return;
    }
    if (!state.previousConnected) {
        state.previousConnected = true;
        state.subscribed = false;
    }
    if (!state.subscribed) state.subscribed = client.subscribe("history/backfill/ack/v1", 1);
    if (!freshLivePublished || !state.subscribed || state.recordCount == 0 ||
        state.waitingForAck || state.replayBlocked) return;
    if (state.lastReplayAttemptMs && millis() - state.lastReplayAttemptMs < REPLAY_RETRY_MS) return;
    state.lastReplayAttemptMs = millis();
    publishBatch(client);
}

void c6OfflineCacheHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length)
{
    if (!state.waitingForAck || topic == nullptr || payload == nullptr || length > 1024) return;
    const String expectedSuffix = "/history/backfill/ack/v1";
    const String receivedTopic(topic);
    if (!receivedTopic.endsWith(expectedSuffix)) return;
    JsonDocument doc;
    if (deserializeJson(doc, payload, length)) return;
    if (doc["version"].as<int>() != 1 || doc["batchId"].as<String>() != state.pendingBatchId) return;
    if (!doc["accepted"].as<bool>()) {
        state.rejectedCount++;
        state.replayBlocked = true;
        state.lastReplayResult = "rejected:" + doc["reason"].as<String>();
        state.waitingForAck = false;
        return;
    }
    if (doc["sampleCount"].as<uint32_t>() != state.pendingBatchCount) return;
    if (!removeAcknowledged(state.pendingBatchCount)) {
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
}

void c6OfflineCachePurge()
{
    if (!state.mounted) state.mounted = LittleFS.begin(false);
    if (state.mounted) {
        LittleFS.remove(CACHE_PATH);
        LittleFS.remove(NEXT_PATH);
    }
    state.recordCount = 0;
    state.oldestSampleMs = 0;
    state.waitingForAck = false;
    state.pendingBatchCount = 0;
    state.pendingBatchId = "";
    state.lastReplayResult = "purged";
}

String c6OfflineCacheStatusJson()
{
    const uint64_t nowMs = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
    const uint64_t oldestAgeSeconds = state.oldestSampleMs && nowMs >= state.oldestSampleMs
        ? (nowMs - state.oldestSampleMs) / 1000ULL : 0;
    String out = "{\"enabled\":" + String(c6Config.offlineCacheEnabled ? "true" : "false");
    out += ",\"cloudPaused\":" + String(!c6Config.motCloudEnabled ? "true" : "false");
    out += ",\"mounted\":" + String(state.mounted ? "true" : "false");
    out += ",\"maxBytes\":" + String(static_cast<unsigned long>(MOT_OFFLINE_CACHE_MAX_BYTES));
    out += ",\"usedBytes\":" + String(static_cast<unsigned long>(state.recordCount * sizeof(CacheRecord)));
    out += ",\"pending\":" + String(state.recordCount);
    out += ",\"oldestAgeSec\":" + String(static_cast<unsigned long long>(oldestAgeSeconds));
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
