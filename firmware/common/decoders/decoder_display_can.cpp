#include "decoder_display_can.h"
#include "../telemetry/telemetry.h"

#include <Arduino.h>

static constexpr float RANGE_FULL_KM = 140.0f;
static constexpr uint8_t CHARGING_POWER_THRESHOLD = 32;

static bool chargingContext()
{
    return telemetry.charging.plugged || telemetry.charging.isCharging ||
           telemetry.bms.plugged || telemetryIsCharging();
}

static void decode601(const uint8_t *b)
{
    telemetry.access.doorOpenValid = true;
    telemetry.access.doorOpen = (b[0] & 0x80) != 0;
    telemetry.access.lastUpdateMs = millis();

    const uint8_t raw = b[3];
    const bool recognized = raw == 0x01 || raw == 0x02;
    if (chargingContext()) {
        telemetry.access.centralLockValid = false;
        telemetry.access.lockSuppressedByCharging = true;
    } else if (recognized) {
        // During charging 0x601 can remain at 0x01 independent of the real
        // electronic lock state. After unplugging, require a new electronic
        // state transition before trusting it again.
        if (!telemetry.access.lockSuppressedByCharging ||
            !telemetry.access.lockRawSeen || raw != telemetry.access.lockRaw) {
            telemetry.access.centralLocked = raw == 0x01;
            telemetry.access.centralLockValid = true;
            telemetry.access.lockSuppressedByCharging = false;
        } else {
            telemetry.access.centralLockValid = false;
        }
    } else {
        telemetry.access.centralLockValid = false;
    }
    telemetry.access.lockRaw = raw;
    telemetry.access.lockRawSeen = true;
}

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

void decoderDisplayCanHandleFrame(const MotCanFrame &frame)
{
    if (frame.extended || frame.dlc < 8) {
        return;
    }

    switch (frame.id) {
        case 0x601: decode601(frame.data); break;
        case 0x602: decode602(frame.data); break;
        case 0x603: decode603(frame.data); break;
        // 0x604 data[4] bit 4 was previously treated as a plugged flag.
        // Controlled unplugged testing disproved that interpretation. Keep the
        // frame out of canonical telemetry; raw capture remains available in
        // the C6 drive diagnostic only.
        case 0x604: break;
        default: break;
    }
}
