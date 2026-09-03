#include "c6_soc_discovery.h"

#include <Arduino.h>
#include <cstring>

#ifdef MOT_SOC_DISCOVERY
namespace {
constexpr size_t STANDARD_ID_COUNT = 0x800;
constexpr size_t SOC_48D_HISTORY_COUNT = 128;

struct FrameState {
    uint32_t frames = 0;
    uint32_t lastFrameMs = 0;
    uint8_t dlc = 0;
    uint8_t last[8] = {0};
};

FrameState states[STANDARD_ID_COUNT];
uint8_t markedPayloads[STANDARD_ID_COUNT][8];
uint8_t markedDlcs[STANDARD_ID_COUNT];
uint8_t displaySocRaw = 0;
uint8_t markedDisplaySocRaw = 0;
bool displaySocValid = false;
bool markValid = false;

struct Soc48dEvent {
    uint32_t timestampMs = 0;
    uint8_t dlc = 0;
    uint8_t data[8] = {0};
    bool displayValid = false;
    uint8_t displayRaw = 0;
};

Soc48dEvent soc48dHistory[SOC_48D_HISTORY_COUNT];
size_t soc48dHistoryNext = 0;
size_t soc48dHistorySize = 0;
bool soc48dSeen = false;
uint8_t soc48dLast6 = 0;
uint8_t soc48dLast7 = 0;

uint16_t readLe16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint16_t readBe16(const uint8_t *data)
{
    return (static_cast<uint16_t>(data[0]) << 8) |
           static_cast<uint16_t>(data[1]);
}

bool matchesDisplayDelta(int32_t rawDelta, int32_t displayDeltaRaw, int scalePerPercent)
{
    return rawDelta * 2 == displayDeltaRaw * scalePerPercent;
}

void printCandidate(uint16_t id, const char *type, uint8_t offset,
                    uint32_t before, uint32_t after, int scale)
{
    Serial.printf("SOC_CANDIDATE id=0x%03X type=%s offset=%u before=%lu after=%lu delta=%ld scale=%d_units_per_percent\n",
                  id, type, offset,
                  static_cast<unsigned long>(before),
                  static_cast<unsigned long>(after),
                  static_cast<long>(static_cast<int32_t>(after) - static_cast<int32_t>(before)),
                  scale);
}

void print48d(const char *prefix, uint32_t timestampMs, uint8_t dlc,
              const uint8_t *data, bool pairedDisplayValid, uint8_t pairedDisplayRaw)
{
    Serial.printf("%s ts=%lu data=", prefix, static_cast<unsigned long>(timestampMs));
    for (uint8_t index = 0; index < dlc; ++index) {
        Serial.printf("%02X%s", data[index], index + 1 < dlc ? " " : "");
    }
    if (dlc > 7) {
        Serial.printf(" byte6=%u byte7=%u", data[6], data[7]);
    }
    if (pairedDisplayValid) {
        Serial.printf(" display=%.1f%%", pairedDisplayRaw / 2.0f);
    }
    Serial.println();
}
}
#endif

void c6SocDiscoveryReset()
{
#ifdef MOT_SOC_DISCOVERY
    memset(states, 0, sizeof(states));
    memset(markedPayloads, 0, sizeof(markedPayloads));
    memset(markedDlcs, 0, sizeof(markedDlcs));
    displaySocRaw = 0;
    markedDisplaySocRaw = 0;
    displaySocValid = false;
    markValid = false;
    memset(soc48dHistory, 0, sizeof(soc48dHistory));
    soc48dHistoryNext = 0;
    soc48dHistorySize = 0;
    soc48dSeen = false;
    Serial.println("SOC discovery reset; recording all standard CAN1 identifiers");
#else
    Serial
        .println("SOC discovery is available only in the dedicated diagnostic build");
#endif
}

void c6SocDiscoveryObserve(size_t channel, const MotCanFrame &frame)
{
#ifdef MOT_SOC_DISCOVERY
    if (frame.extended || frame.dlc == 0) return;
    if (channel == 1 && frame.id == 0x602 && frame.dlc >= 1) {
        displaySocRaw = frame.data[0];
        displaySocValid = true;
        return;
    }
    if (channel != 0 || frame.id >= STANDARD_ID_COUNT) return;

    FrameState &state = states[frame.id];
    state.frames++;
    state.lastFrameMs = frame.receivedMs;
    state.dlc = frame.dlc < 8 ? frame.dlc : 8;
    memcpy(state.last, frame.data, state.dlc);

    if (frame.id == 0x48D && state.dlc >= 8 &&
        (!soc48dSeen || frame.data[6] != soc48dLast6 || frame.data[7] != soc48dLast7)) {
        Soc48dEvent &event = soc48dHistory[soc48dHistoryNext];
        event.timestampMs = frame.receivedMs;
        event.dlc = state.dlc;
        memcpy(event.data, frame.data, event.dlc);
        event.displayValid = displaySocValid;
        event.displayRaw = displaySocRaw;
        soc48dHistoryNext = (soc48dHistoryNext + 1) % SOC_48D_HISTORY_COUNT;
        if (soc48dHistorySize < SOC_48D_HISTORY_COUNT) soc48dHistorySize++;
        soc48dLast6 = frame.data[6];
        soc48dLast7 = frame.data[7];
        soc48dSeen = true;
    }
#else
    (void)channel;
    (void)frame;
#endif
}

void c6SocDiscoveryMark()
{
#ifdef MOT_SOC_DISCOVERY
    if (!displaySocValid) {
        Serial.println("SOC mark failed: no Display-CAN 0x602 SOC received");
        return;
    }
    size_t seen = 0;
    for (size_t id = 0; id < STANDARD_ID_COUNT; ++id) {
        const FrameState &state = states[id];
        markedDlcs[id] = state.frames ? state.dlc : 0;
        if (!state.frames) continue;
        memcpy(markedPayloads[id], state.last, state.dlc);
        seen++;
    }
    markedDisplaySocRaw = displaySocRaw;
    markValid = true;
    Serial.printf("SOC mark stored display=%.1f%% raw=%u standard_ids=%u\n",
                  markedDisplaySocRaw / 2.0f, markedDisplaySocRaw,
                  static_cast<unsigned>(seen));
    const FrameState &soc48d = states[0x48D];
    if (soc48d.frames) {
        print48d("SOC_48D_MARK", soc48d.lastFrameMs, soc48d.dlc, soc48d.last,
                 displaySocValid, displaySocRaw);
    }
#else
    Serial.println("SOC mark is available only in the dedicated diagnostic build");
#endif
}

void c6SocDiscoveryDump()
{
#ifdef MOT_SOC_DISCOVERY
    if (!markValid || !displaySocValid) {
        Serial.println("SOC dump failed: run 'soc mark' with valid Display-CAN first");
        return;
    }
    const int32_t displayDeltaRaw =
        static_cast<int32_t>(displaySocRaw) - markedDisplaySocRaw;
    Serial.printf("SOC_COMPARE display_before=%.1f%% display_after=%.1f%% raw_delta=%ld\n",
                  markedDisplaySocRaw / 2.0f, displaySocRaw / 2.0f,
                  static_cast<long>(displayDeltaRaw));
    if (markedDlcs[0x48D]) {
        print48d("SOC_48D_BEFORE", 0, markedDlcs[0x48D], markedPayloads[0x48D],
                 true, markedDisplaySocRaw);
    }
    const FrameState &soc48d = states[0x48D];
    if (soc48d.frames) {
        print48d("SOC_48D_AFTER", soc48d.lastFrameMs, soc48d.dlc, soc48d.last,
                 displaySocValid, displaySocRaw);
    }
    if (displayDeltaRaw == 0) {
        Serial.println("SOC_COMPARE no display change; candidates require a changed SOC");
        return;
    }

    size_t candidates = 0;
    constexpr int scales[] = {1, 2, 10, 100};
    for (uint16_t id = 0; id < STANDARD_ID_COUNT; ++id) {
        const FrameState &state = states[id];
        const uint8_t length = min(markedDlcs[id], state.dlc);
        if (!state.frames || length == 0) continue;
        for (uint8_t offset = 0; offset < length; ++offset) {
            const uint8_t before = markedPayloads[id][offset];
            const uint8_t after = state.last[offset];
            const int32_t delta = static_cast<int32_t>(after) - before;
            for (int scale : scales) {
                if (matchesDisplayDelta(delta, displayDeltaRaw, scale)) {
                    printCandidate(id, "u8", offset, before, after, scale);
                    candidates++;
                }
            }
        }
        for (uint8_t offset = 0; offset + 1 < length; ++offset) {
            const uint16_t beforeLe = readLe16(&markedPayloads[id][offset]);
            const uint16_t afterLe = readLe16(&state.last[offset]);
            const uint16_t beforeBe = readBe16(&markedPayloads[id][offset]);
            const uint16_t afterBe = readBe16(&state.last[offset]);
            for (int scale : scales) {
                if (matchesDisplayDelta(static_cast<int32_t>(afterLe) - beforeLe,
                                        displayDeltaRaw, scale)) {
                    printCandidate(id, "u16le", offset, beforeLe, afterLe, scale);
                    candidates++;
                }
                if (matchesDisplayDelta(static_cast<int32_t>(afterBe) - beforeBe,
                                        displayDeltaRaw, scale)) {
                    printCandidate(id, "u16be", offset, beforeBe, afterBe, scale);
                    candidates++;
                }
            }
        }
    }
    Serial.printf("SOC_COMPARE candidates=%u\n", static_cast<unsigned>(candidates));
#else
    Serial.println("SOC dump is available only in the dedicated diagnostic build");
#endif
}


void c6SocDiscovery48dDump()
{
#ifdef MOT_SOC_DISCOVERY
    Serial.printf("SOC_48D_HISTORY events=%u capacity=%u%s\n",
                  static_cast<unsigned>(soc48dHistorySize),
                  static_cast<unsigned>(SOC_48D_HISTORY_COUNT),
                  soc48dHistorySize == SOC_48D_HISTORY_COUNT ? " wrapped=yes" : "");
    const size_t start = (soc48dHistoryNext + SOC_48D_HISTORY_COUNT - soc48dHistorySize) %
                         SOC_48D_HISTORY_COUNT;
    for (size_t index = 0; index < soc48dHistorySize; ++index) {
        const Soc48dEvent &event = soc48dHistory[(start + index) % SOC_48D_HISTORY_COUNT];
        print48d("SOC_48D_EVENT", event.timestampMs, event.dlc, event.data,
                 event.displayValid, event.displayRaw);
    }
#else
    Serial.println("SOC 0x48D history is available only in the dedicated diagnostic build");
#endif
}
