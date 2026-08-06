#include "c6_can_scan.h"

#include <Arduino.h>
#include <cstring>

namespace {
constexpr uint32_t TARGET_IDS[] = {
    0x101, 0x107, 0x18D, 0x1B0, 0x1B1, 0x2BA, 0x37F, 0x4AD
};

struct ScanState {
    uint32_t frames = 0;
    uint32_t lastFrameMs = 0;
    uint8_t dlc = 0;
    uint8_t first[8] = {0};
    uint8_t last[8] = {0};
    uint8_t minimum[8] = {0};
    uint8_t maximum[8] = {0};
    uint8_t changedMask = 0;
};

ScanState states[sizeof(TARGET_IDS) / sizeof(TARGET_IDS[0])];

int targetIndex(uint32_t id)
{
    for (size_t index = 0; index < sizeof(TARGET_IDS) / sizeof(TARGET_IDS[0]); ++index) {
        if (TARGET_IDS[index] == id) return static_cast<int>(index);
    }
    return -1;
}

void printBytes(const uint8_t *data, uint8_t length)
{
    for (uint8_t index = 0; index < length; ++index) {
        Serial.printf("%02X%s", data[index], index + 1 < length ? " " : "");
    }
}
}

void c6CanScanReset()
{
    memset(states, 0, sizeof(states));
    Serial.println("CAN1 targeted scan reset");
}

void c6CanScanObserve(size_t channel, const MotCanFrame &frame)
{
    if (channel != 0 || frame.extended) return;

    const int index = targetIndex(frame.id);
    if (index < 0) return;

    ScanState &state = states[index];
    const uint8_t length = frame.dlc < 8 ? frame.dlc : 8;
    if (state.frames == 0) {
        state.dlc = length;
        memcpy(state.first, frame.data, length);
        memcpy(state.last, frame.data, length);
        memcpy(state.minimum, frame.data, length);
        memcpy(state.maximum, frame.data, length);
    } else {
        if (state.dlc != length) state.changedMask = 0xFF;
        for (uint8_t byte = 0; byte < length; ++byte) {
            if (state.last[byte] != frame.data[byte]) {
                state.changedMask |= static_cast<uint8_t>(1U << byte);
            }
            state.minimum[byte] = min(state.minimum[byte], frame.data[byte]);
            state.maximum[byte] = max(state.maximum[byte], frame.data[byte]);
            state.last[byte] = frame.data[byte];
        }
        state.dlc = length;
    }
    state.frames++;
    state.lastFrameMs = frame.receivedMs;
}

void c6CanScanDump()
{
    Serial.println("CAN1 targeted scan dump (first -> last; min/max; changed byte mask)");
    for (size_t index = 0; index < sizeof(TARGET_IDS) / sizeof(TARGET_IDS[0]); ++index) {
        const ScanState &state = states[index];
        Serial.printf("SCAN 0x%03lX frames=%lu age=%lu ms ",
                      static_cast<unsigned long>(TARGET_IDS[index]),
                      static_cast<unsigned long>(state.frames),
                      state.frames ? static_cast<unsigned long>(millis() - state.lastFrameMs) : 0UL);
        if (state.frames == 0) {
            Serial.println("not-seen");
            continue;
        }
        Serial.print("first=");
        printBytes(state.first, state.dlc);
        Serial.print(" last=");
        printBytes(state.last, state.dlc);
        Serial.print(" min=");
        printBytes(state.minimum, state.dlc);
        Serial.print(" max=");
        printBytes(state.maximum, state.dlc);
        Serial.printf(" changed=0x%02X\n", state.changedMask);
    }
}
