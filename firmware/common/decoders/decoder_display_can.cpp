#include "decoder_display_can.h"
#include "../telemetry/telemetry.h"

#include <Arduino.h>

static constexpr float RANGE_FULL_KM = 140.0f;
static constexpr uint8_t CHARGING_POWER_THRESHOLD = 32;

static void decode602(const uint8_t *b)
{
    telemetry.display.valid = true;
    telemetry.display.lastUpdateMs = millis();

    telemetry.display.soc = b[0] / 2.0f;
    telemetry.display.speedKmh = b[1] / 2.0f;

    uint32_t odoRaw =
        ((uint32_t)(b[7] & 0x0F) << 24) |
        ((uint32_t)b[6] << 16) |
        ((uint32_t)b[5] << 8) |
        b[4];

    telemetry.display.odometerKm = odoRaw / 1024.0f;
    telemetry.display.estimatedRangeKm = roundf(RANGE_FULL_KM * telemetry.display.soc / 100.0f);
}

static void decode603(const uint8_t *b)
{
    telemetry.charging.valid = true;
    telemetry.charging.lastUpdateMs = millis();
    telemetry.charging.powerDisplay = b[4];

    bool regenCandidate = b[6] & 0x40;
    telemetry.charging.powerSigned = regenCandidate ? -(int)b[4] : (int)b[4];

    // Threshold is intentionally conservative; values must be calibrated with real charging sessions.
    telemetry.charging.isCharging = b[4] >= CHARGING_POWER_THRESHOLD;
}

static void decode604(const uint8_t *b)
{
    telemetry.charging.plugged = b[4] & 0x10;
}

void decoderDisplayCanHandleFrame(const MotCanFrame &frame)
{
    if (frame.extended || frame.dlc < 8) {
        return;
    }

    switch (frame.id) {
        case 0x602: decode602(frame.data); break;
        case 0x603: decode603(frame.data); break;
        case 0x604: decode604(frame.data); break;
        default: break;
    }
}
