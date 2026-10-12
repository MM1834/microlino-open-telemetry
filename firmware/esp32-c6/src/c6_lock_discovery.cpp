#include "c6_lock_discovery.h"

#include <Arduino.h>
#include <cstring>

#ifdef MOT_LOCK_DISCOVERY
namespace {
constexpr size_t CHANNEL_COUNT = 2;
constexpr size_t STANDARD_ID_COUNT = 0x800;
constexpr size_t EVENT_COUNT = 1024;
constexpr uint8_t EVENTS_PER_ID = 32;

struct FrameState {
    uint32_t frames = 0;
    uint32_t lastFrameMs = 0;
    uint8_t dlc = 0;
    uint8_t data[8] = {0};
};

struct ChangeEvent {
    uint32_t timestampMs = 0;
    uint16_t id = 0;
    uint8_t channel = 0;
    uint8_t dlc = 0;
    uint8_t changedMask = 0;
    uint8_t before[8] = {0};
    uint8_t after[8] = {0};
};

FrameState states[CHANNEL_COUNT][STANDARD_ID_COUNT];
uint8_t markedPayloads[CHANNEL_COUNT][STANDARD_ID_COUNT][8];
uint8_t markedDlcs[CHANNEL_COUNT][STANDARD_ID_COUNT];
ChangeEvent events[EVENT_COUNT];
size_t eventNext = 0;
size_t eventSize = 0;
uint32_t overwrittenEvents = 0;
uint32_t limitedEvents = 0;
uint8_t eventCounts[CHANNEL_COUNT][STANDARD_ID_COUNT];
bool recording = false;
bool markValid = false;

void printPayload(const uint8_t *data, uint8_t dlc)
{
    for (uint8_t offset = 0; offset < dlc; ++offset) {
        Serial.printf("%02X%s", data[offset], offset + 1 < dlc ? " " : "");
    }
}

uint8_t payloadChangedMask(const uint8_t *before, uint8_t beforeDlc,
                           const uint8_t *after, uint8_t afterDlc)
{
    if (beforeDlc != afterDlc) return 0xFF;
    uint8_t mask = 0;
    for (uint8_t offset = 0; offset < afterDlc; ++offset) {
        if (before[offset] != after[offset]) mask |= static_cast<uint8_t>(1U << offset);
    }
    return mask;
}

void storeEvent(size_t channel, const MotCanFrame &frame, const FrameState &before,
                uint8_t dlc, uint8_t mask)
{
    ChangeEvent &event = events[eventNext];
    event.timestampMs = frame.receivedMs;
    event.id = static_cast<uint16_t>(frame.id);
    event.channel = static_cast<uint8_t>(channel);
    event.dlc = dlc;
    event.changedMask = mask;
    memset(event.before, 0, sizeof(event.before));
    memset(event.after, 0, sizeof(event.after));
    if (before.frames) memcpy(event.before, before.data, before.dlc);
    memcpy(event.after, frame.data, dlc);
    eventNext = (eventNext + 1) % EVENT_COUNT;
    if (eventSize < EVENT_COUNT) eventSize++;
    else overwrittenEvents++;
}

void storeMark()
{
    size_t seen[CHANNEL_COUNT] = {0, 0};
    for (size_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
        for (size_t id = 0; id < STANDARD_ID_COUNT; ++id) {
            const FrameState &state = states[channel][id];
            markedDlcs[channel][id] = state.frames ? state.dlc : 0;
            if (!state.frames) continue;
            memcpy(markedPayloads[channel][id], state.data, state.dlc);
            seen[channel]++;
        }
    }
    markValid = true;
    Serial.printf("LOCK mark stored CAN1_ids=%u CAN2_ids=%u\n",
                  static_cast<unsigned>(seen[0]), static_cast<unsigned>(seen[1]));
}
}
#endif

void c6LockDiscoveryReset()
{
#ifdef MOT_LOCK_DISCOVERY
    memset(states, 0, sizeof(states));
    memset(markedPayloads, 0, sizeof(markedPayloads));
    memset(markedDlcs, 0, sizeof(markedDlcs));
    memset(events, 0, sizeof(events));
    eventNext = 0;
    eventSize = 0;
    overwrittenEvents = 0;
    limitedEvents = 0;
    memset(eventCounts, 0, sizeof(eventCounts));
    recording = false;
    markValid = false;
    Serial.println("LOCK discovery reset; observing all 11-bit frames on CAN1 and CAN2");
#else
    Serial.println("LOCK discovery is available only in the dedicated diagnostic build");
#endif
}

void c6LockDiscoveryObserve(size_t channel, const MotCanFrame &frame)
{
#ifdef MOT_LOCK_DISCOVERY
    if (channel >= CHANNEL_COUNT || frame.extended || frame.id >= STANDARD_ID_COUNT ||
        frame.dlc == 0) return;
    const uint8_t dlc = frame.dlc < 8 ? frame.dlc : 8;
    FrameState &state = states[channel][frame.id];
    const uint8_t mask = state.frames
        ? payloadChangedMask(state.data, state.dlc, frame.data, dlc)
        : 0xFF;
    if (recording && mask) {
        uint8_t &count = eventCounts[channel][frame.id];
        if (count < EVENTS_PER_ID) {
            storeEvent(channel, frame, state, dlc, mask);
            count++;
        } else {
            limitedEvents++;
        }
    }
    state.frames++;
    state.lastFrameMs = frame.receivedMs;
    state.dlc = dlc;
    memcpy(state.data, frame.data, dlc);
#else
    (void)channel;
    (void)frame;
#endif
}

void c6LockDiscoveryStart()
{
#ifdef MOT_LOCK_DISCOVERY
    memset(events, 0, sizeof(events));
    eventNext = 0;
    eventSize = 0;
    overwrittenEvents = 0;
    limitedEvents = 0;
    memset(eventCounts, 0, sizeof(eventCounts));
    storeMark();
    recording = true;
    Serial.println("LOCK capture started");
#else
    Serial.println("LOCK capture is available only in the dedicated diagnostic build");
#endif
}

void c6LockDiscoveryStop()
{
#ifdef MOT_LOCK_DISCOVERY
    recording = false;
    Serial.printf("LOCK capture stopped events=%u capacity=%u overwritten=%lu per_id_limited=%lu\n",
                  static_cast<unsigned>(eventSize), static_cast<unsigned>(EVENT_COUNT),
                  static_cast<unsigned long>(overwrittenEvents),
                  static_cast<unsigned long>(limitedEvents));
#else
    Serial.println("LOCK capture is available only in the dedicated diagnostic build");
#endif
}

void c6LockDiscoveryDump()
{
#ifdef MOT_LOCK_DISCOVERY
    Serial.printf("LOCK_EVENTS events=%u capacity=%u overwritten=%lu per_id_limited=%lu recording=%s\n",
                  static_cast<unsigned>(eventSize), static_cast<unsigned>(EVENT_COUNT),
                  static_cast<unsigned long>(overwrittenEvents),
                  static_cast<unsigned long>(limitedEvents), recording ? "yes" : "no");
    const size_t start = (eventNext + EVENT_COUNT - eventSize) % EVENT_COUNT;
    for (size_t offset = 0; offset < eventSize; ++offset) {
        const ChangeEvent &event = events[(start + offset) % EVENT_COUNT];
        Serial.printf("LOCK_EVENT ts=%lu can=%u id=0x%03X changed=0x%02X before=",
                      static_cast<unsigned long>(event.timestampMs), event.channel + 1,
                      event.id, event.changedMask);
        printPayload(event.before, event.dlc);
        Serial.print(" after=");
        printPayload(event.after, event.dlc);
        Serial.println();
    }
#else
    Serial.println("LOCK capture is available only in the dedicated diagnostic build");
#endif
}

void c6LockDiscoveryDiff()
{
#ifdef MOT_LOCK_DISCOVERY
    if (!markValid) {
        Serial.println("LOCK diff failed: run 'lock start' first");
        return;
    }
    size_t changedIds = 0;
    for (size_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
        for (uint16_t id = 0; id < STANDARD_ID_COUNT; ++id) {
            const FrameState &state = states[channel][id];
            if (!state.frames) continue;
            const uint8_t beforeDlc = markedDlcs[channel][id];
            const uint8_t mask = payloadChangedMask(markedPayloads[channel][id], beforeDlc,
                                                    state.data, state.dlc);
            if (beforeDlc && !mask) continue;
            Serial.printf("LOCK_DIFF can=%u id=0x%03X age=%lu changed=0x%02X before=",
                          static_cast<unsigned>(channel + 1), id,
                          static_cast<unsigned long>(millis() - state.lastFrameMs), mask);
            if (beforeDlc) printPayload(markedPayloads[channel][id], beforeDlc);
            else Serial.print("not-seen");
            Serial.print(" after=");
            printPayload(state.data, state.dlc);
            Serial.println();
            changedIds++;
        }
    }
    Serial.printf("LOCK_DIFF changed_ids=%u\n", static_cast<unsigned>(changedIds));
#else
    Serial.println("LOCK diff is available only in the dedicated diagnostic build");
#endif
}
