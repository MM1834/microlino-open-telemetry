#include "c6_vcu_diagnostic.h"

#include <cstring>

#ifdef MOT_VCU_DIAGNOSTIC
namespace {

struct FrameState {
    uint32_t frames = 0;
    uint32_t firstFrameMs = 0;
    uint32_t lastFrameMs = 0;
    uint8_t dlc = 0;
    uint8_t first[8] = {0};
    uint8_t last[8] = {0};
    uint8_t minimum[8] = {0};
    uint8_t maximum[8] = {0};
    uint8_t changedMask = 0;
};

FrameState standardTemperatures;
FrameState standardVcuVersion;
FrameState displayVcuVersion;

uint16_t readLe16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

void observe(FrameState &state, const MotCanFrame &frame)
{
    const uint8_t length = frame.dlc < 8 ? frame.dlc : 8;
    if (state.frames == 0) {
        state.firstFrameMs = frame.receivedMs;
        state.dlc = length;
        memcpy(state.first, frame.data, length);
        memcpy(state.last, frame.data, length);
        memcpy(state.minimum, frame.data, length);
        memcpy(state.maximum, frame.data, length);
    } else {
        if (state.dlc != length) state.changedMask = 0xFF;
        for (uint8_t index = 0; index < length; ++index) {
            if (state.last[index] != frame.data[index]) {
                state.changedMask |= static_cast<uint8_t>(1U << index);
            }
            state.minimum[index] = min(state.minimum[index], frame.data[index]);
            state.maximum[index] = max(state.maximum[index], frame.data[index]);
            state.last[index] = frame.data[index];
        }
        state.dlc = length;
    }
    state.frames++;
    state.lastFrameMs = frame.receivedMs;
}

void printBytes(const uint8_t *data, uint8_t length)
{
    for (uint8_t index = 0; index < length; ++index) {
        Serial.printf("%02X%s", data[index], index + 1 < length ? " " : "");
    }
}

void printFrame(const char *label, uint8_t channel, uint16_t id,
                const FrameState &state)
{
    Serial.printf("VCU_DIAG %s can=%u id=0x%03X frames=%lu ", label,
                  channel, id, static_cast<unsigned long>(state.frames));
    if (state.frames == 0) {
        Serial.println("not-seen");
        return;
    }
    Serial.printf("first_ms=%lu last_ms=%lu age_ms=%lu changed=0x%02X first=",
                  static_cast<unsigned long>(state.firstFrameMs),
                  static_cast<unsigned long>(state.lastFrameMs),
                  static_cast<unsigned long>(millis() - state.lastFrameMs),
                  state.changedMask);
    printBytes(state.first, state.dlc);
    Serial.print(" last=");
    printBytes(state.last, state.dlc);
    Serial.print(" min=");
    printBytes(state.minimum, state.dlc);
    Serial.print(" max=");
    printBytes(state.maximum, state.dlc);
    Serial.println();
}

void printTemperatureCandidates(const FrameState &state)
{
    if (state.frames == 0 || state.dlc < 8) return;
    const uint16_t motorRaw = readLe16(&state.last[0]);
    const uint16_t inverter1Raw = readLe16(&state.last[4]);
    const uint16_t inverter2Raw = readLe16(&state.last[6]);
    Serial.printf(
        "VCU_TEMP_CANDIDATE id=0x105 motor_raw=%u motor_c=%.2f "
        "inverter1_raw=%u inverter1_c=%.2f inverter2_raw=%u inverter2_c=%.2f\n",
        motorRaw, motorRaw * 0.0178f - 21.5f,
        inverter1Raw, inverter1Raw * 0.016f - 14.0f,
        inverter2Raw, inverter2Raw * 0.016f - 14.0f);
}

void printVersionCandidate(const char *source, uint16_t id,
                           const FrameState &state)
{
    if (state.frames == 0 || state.dlc < 6) return;
    const uint16_t build = readLe16(&state.last[4]);
    Serial.printf(
        "VCU_VERSION_CANDIDATE source=%s id=0x%03X major=%u minor=%u build=%u raw=",
        source, id, state.last[1], state.last[0], build);
    printBytes(state.last, state.dlc);
    Serial.println();
}

}  // namespace
#endif

void c6VcuDiagnosticObserve(size_t channel, const MotCanFrame &frame)
{
#ifdef MOT_VCU_DIAGNOSTIC
    if (frame.extended) return;
    if (channel == 0 && frame.id == 0x105) {
        observe(standardTemperatures, frame);
    } else if (channel == 0 && frame.id == 0x700) {
        observe(standardVcuVersion, frame);
    } else if (channel == 1 && frame.id == 0x606) {
        observe(displayVcuVersion, frame);
    }
#else
    (void)channel;
    (void)frame;
#endif
}

void c6VcuDiagnosticReset()
{
#ifdef MOT_VCU_DIAGNOSTIC
    memset(&standardTemperatures, 0, sizeof(standardTemperatures));
    memset(&standardVcuVersion, 0, sizeof(standardVcuVersion));
    memset(&displayVcuVersion, 0, sizeof(displayVcuVersion));
    Serial.println("VCU diagnostic reset; passive RAM-only capture active");
#else
    Serial.println("VCU diagnostic is available only in the dedicated diagnostic build");
#endif
}

void c6VcuDiagnosticDump()
{
#ifdef MOT_VCU_DIAGNOSTIC
    printFrame("temperatures", 1, 0x105, standardTemperatures);
    printTemperatureCandidates(standardTemperatures);
    printFrame("version_standard", 1, 0x700, standardVcuVersion);
    printVersionCandidate("standard-can", 0x700, standardVcuVersion);
    printFrame("version_display", 2, 0x606, displayVcuVersion);
    printVersionCandidate("display-can", 0x606, displayVcuVersion);
    if (standardVcuVersion.frames && displayVcuVersion.frames) {
        const bool same = standardVcuVersion.dlc == displayVcuVersion.dlc &&
                          memcmp(standardVcuVersion.last, displayVcuVersion.last,
                                 standardVcuVersion.dlc) == 0;
        Serial.printf("VCU_VERSION_COMPARE byte_identical=%s\n", same ? "true" : "false");
    }
#else
    Serial.println("VCU diagnostic is available only in the dedicated diagnostic build");
#endif
}
