#include "c6_data_volume.h"

#ifdef MOT_DATA_VOLUME_ANALYSIS_BUILD

#include <Preferences.h>
#include <time.h>

#include "abrp/abrp_client.h"
#include "c6_abrp.h"
#include "c6_aws.h"
#include "c6_local_mqtt.h"
#include "c6_network.h"

namespace {
constexpr uint32_t MAGIC = 0x4D4F5456;
constexpr uint16_t SCHEMA = 1;
constexpr uint32_t CHECKPOINT_MS = 10UL * 60UL * 1000UL;
constexpr time_t MIN_VALID_TIME = 1700000000;
constexpr char NAMESPACE[] = "mot-volume";
constexpr char KEY[] = "snapshot";

struct Counters {
    uint64_t awsPublishes = 0;
    uint64_t awsPayloadBytes = 0;
    uint64_t awsMqttPublishBytes = 0;
    uint64_t awsConnectAttempts = 0;
    uint64_t awsReconnects = 0;
    uint64_t awsDisconnects = 0;
    uint64_t localPublishes = 0;
    uint64_t localPayloadBytes = 0;
    uint64_t localMqttPublishBytes = 0;
    uint64_t localConnectAttempts = 0;
    uint64_t localConnectSuccesses = 0;
    uint64_t localDisconnects = 0;
    uint64_t abrpAttempts = 0;
    uint64_t abrpSuccesses = 0;
    uint64_t abrpFailures = 0;
    uint64_t abrpRequestBytes = 0;
    uint64_t abrpResponseBytes = 0;
    uint64_t networkDisconnects = 0;
};

struct Snapshot {
    uint32_t magic = MAGIC;
    uint16_t schema = SCHEMA;
    uint16_t size = sizeof(Snapshot);
    uint64_t startedAt = 0;
    uint64_t lastCheckpointAt = 0;
    uint32_t bootCount = 0;
    uint32_t checkpointCount = 0;
    Counters totals;
};

Preferences preferences;
Snapshot snapshot;
Counters lastRuntime;
uint32_t lastCheckpointMs = 0;
bool dirty = false;
bool restored = false;

uint64_t validEpoch()
{
    const time_t now = time(nullptr);
    return now >= MIN_VALID_TIME ? static_cast<uint64_t>(now) : 0;
}

Counters runtimeCounters()
{
    Counters value;
    uint32_t count = 0, attempts = 0, successes = 0, reconnects = 0, disconnects = 0;
    uint64_t payload = 0, mqtt = 0;
    c6AwsVolumeCounters(count, payload, mqtt, attempts, reconnects, disconnects);
    value.awsPublishes = count;
    value.awsPayloadBytes = payload;
    value.awsMqttPublishBytes = mqtt;
    value.awsConnectAttempts = attempts;
    value.awsReconnects = reconnects;
    value.awsDisconnects = disconnects;
    c6LocalMqttVolumeCounters(count, payload, mqtt, attempts, successes, disconnects);
    value.localPublishes = count;
    value.localPayloadBytes = payload;
    value.localMqttPublishBytes = mqtt;
    value.localConnectAttempts = attempts;
    value.localConnectSuccesses = successes;
    value.localDisconnects = disconnects;
    const AbrpStatus abrp = abrpStatus(AbrpSettings{});
    value.abrpAttempts = abrp.attemptCount;
    value.abrpSuccesses = abrp.successCount;
    value.abrpFailures = abrp.failureCount;
    value.abrpRequestBytes = abrp.requestBytes;
    value.abrpResponseBytes = abrp.responseBytes;
    value.networkDisconnects = c6NetworkDisconnectCount();
    return value;
}

void addDelta(uint64_t &total, uint64_t current, uint64_t &previous)
{
    if (current >= previous) total += current - previous;
    else total += current;
    if (current != previous) dirty = true;
    previous = current;
}

void collect()
{
    Counters current = runtimeCounters();
#define ADD(field) addDelta(snapshot.totals.field, current.field, lastRuntime.field)
    ADD(awsPublishes); ADD(awsPayloadBytes); ADD(awsMqttPublishBytes);
    ADD(awsConnectAttempts); ADD(awsReconnects); ADD(awsDisconnects);
    ADD(localPublishes); ADD(localPayloadBytes); ADD(localMqttPublishBytes);
    ADD(localConnectAttempts); ADD(localConnectSuccesses); ADD(localDisconnects);
    ADD(abrpAttempts); ADD(abrpSuccesses); ADD(abrpFailures);
    ADD(abrpRequestBytes); ADD(abrpResponseBytes); ADD(networkDisconnects);
#undef ADD
    const uint64_t now = validEpoch();
    if (!snapshot.startedAt && now) { snapshot.startedAt = now; dirty = true; }
}

bool saveSnapshot(bool force)
{
    collect();
    if (!force && (!dirty || millis() - lastCheckpointMs < CHECKPOINT_MS)) return true;
    const uint64_t now = validEpoch();
    if (now) snapshot.lastCheckpointAt = now;
    snapshot.checkpointCount++;
    const bool ok = preferences.putBytes(KEY, &snapshot, sizeof(snapshot)) == sizeof(snapshot);
    if (ok) { dirty = false; lastCheckpointMs = millis(); }
    return ok;
}

String u64(uint64_t value)
{
    char text[24];
    snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(value));
    return String(text);
}
}

void c6DataVolumeSetup()
{
    preferences.begin(NAMESPACE, false);
    Snapshot stored;
    if (preferences.getBytesLength(KEY) == sizeof(stored) &&
        preferences.getBytes(KEY, &stored, sizeof(stored)) == sizeof(stored) &&
        stored.magic == MAGIC && stored.schema == SCHEMA && stored.size == sizeof(stored)) {
        snapshot = stored;
        restored = true;
    }
    snapshot.bootCount++;
    lastRuntime = runtimeCounters();
    dirty = true;
    saveSnapshot(true);
}

void c6DataVolumeLoop() { saveSnapshot(false); }

bool c6DataVolumeReset()
{
    snapshot = Snapshot{};
    snapshot.startedAt = validEpoch();
    snapshot.bootCount = 1;
    lastRuntime = runtimeCounters();
    restored = false;
    dirty = true;
    return saveSnapshot(true);
}

String c6DataVolumeStatusJson()
{
    collect();
    const Counters &t = snapshot.totals;
    String out = "{\"enabled\":true,\"persistent\":true,\"measurementBasis\":\"application-and-mqtt-publish\",\"checkpointIntervalSec\":600";
    out += ",\"restored\":" + String(restored ? "true" : "false");
    out += ",\"startedAt\":" + u64(snapshot.startedAt);
    out += ",\"lastCheckpointAt\":" + u64(snapshot.lastCheckpointAt);
    out += ",\"bootCount\":" + String(snapshot.bootCount);
    out += ",\"checkpointCount\":" + String(snapshot.checkpointCount);
    out += ",\"aws\":{\"publishCount\":" + u64(t.awsPublishes) + ",\"payloadBytes\":" + u64(t.awsPayloadBytes) + ",\"mqttPublishBytes\":" + u64(t.awsMqttPublishBytes) + ",\"connectAttempts\":" + u64(t.awsConnectAttempts) + ",\"reconnects\":" + u64(t.awsReconnects) + ",\"disconnects\":" + u64(t.awsDisconnects) + "}";
    out += ",\"localMqtt\":{\"publishCount\":" + u64(t.localPublishes) + ",\"payloadBytes\":" + u64(t.localPayloadBytes) + ",\"mqttPublishBytes\":" + u64(t.localMqttPublishBytes) + ",\"connectAttempts\":" + u64(t.localConnectAttempts) + ",\"connectSuccesses\":" + u64(t.localConnectSuccesses) + ",\"disconnects\":" + u64(t.localDisconnects) + "}";
    out += ",\"abrp\":{\"attemptCount\":" + u64(t.abrpAttempts) + ",\"successCount\":" + u64(t.abrpSuccesses) + ",\"failureCount\":" + u64(t.abrpFailures) + ",\"requestBytes\":" + u64(t.abrpRequestBytes) + ",\"responseBytes\":" + u64(t.abrpResponseBytes) + "}";
    out += ",\"network\":{\"disconnects\":" + u64(t.networkDisconnects) + "}}";
    return out;
}

#else

void c6DataVolumeSetup() {}
void c6DataVolumeLoop() {}
String c6DataVolumeStatusJson() { return "{\"enabled\":false}"; }
bool c6DataVolumeReset() { return false; }

#endif
