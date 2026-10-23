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

    // Intel-order DBC fields: trip 28|14 at 0.1 km and odometer 42|18 at 1 km.
    const uint16_t tripRaw = static_cast<uint16_t>(b[3] >> 4) |
        (static_cast<uint16_t>(b[4]) << 4) |
        (static_cast<uint16_t>(b[5] & 0x03U) << 12);
    const uint32_t odoRaw = static_cast<uint32_t>(b[5] >> 2) |
        (static_cast<uint32_t>(b[6]) << 6) |
        (static_cast<uint32_t>(b[7] & 0x0FU) << 14);
    telemetry.display.tripKm = tripRaw / 10.0f;
    telemetry.display.odometerKm = static_cast<float>(odoRaw);
    const uint8_t gear = (b[7] >> 6) & 0x03U;
    telemetry.display.gearStateValid = gear <= 2U;
    telemetry.display.gearState = gear;
    telemetry.display.candidateStateLastUpdateMs = millis();
    telemetry.display.estimatedRangeKm = roundf(RANGE_FULL_KM * telemetry.display.soc / 100.0f);
}

void decoderDisplayCanHandleLocalStatusFrame(const MotCanFrame &frame)
{
    if (frame.extended || frame.dlc < 8 || frame.id != 0x603) return;
    const uint8_t *b = frame.data;
    // DBC start bits 17/18/19 are Intel-order bits 1/2/3 in data byte 2.
    // Some dashboards may signal hazards only through the dedicated hazard
    // bit, so make that state illuminate both direction indicators.
    const bool hazardsActive = (b[2] & 0x08U) != 0;
    telemetry.display.indicatorStateValid = true;
    telemetry.display.leftIndicatorActive = hazardsActive || (b[2] & 0x04U) != 0;
    telemetry.display.rightIndicatorActive = hazardsActive || (b[2] & 0x02U) != 0;
    telemetry.display.indicatorLastUpdateMs = millis();

    // Additional local-only dashboard states from Intel-order DBC start bits:
    // 20 = byte 2 bit 4, 24 = complete byte 3, 43 = byte 5 bit 3,
    // 45 = byte 5 bit 5. These values are intentionally not canonical cloud
    // telemetry; they exist only for the directly paired MOT Display.
    telemetry.display.safetyStateValid = true;
    telemetry.display.parkingBrakeActive = (b[2] & 0x10U) != 0;
    telemetry.display.warning90Code = b[3];
    telemetry.display.warningHblAhActive = (b[5] & 0x08U) != 0;
    telemetry.display.drivetrainWarningYellow = (b[5] & 0x20U) != 0;
    telemetry.display.safetyLastUpdateMs = millis();

    // External DBC discovery fields. Keep their validity separate until the
    // light and Sport/Eco mappings have passed controlled vehicle tests.
    telemetry.display.lightStateValid = true;
    telemetry.display.positionLightActive = (b[2] & 0x40U) != 0;
    telemetry.display.lowBeamActive = (b[2] & 0x80U) != 0;
    telemetry.display.driveModeStateValid = true;
    telemetry.display.sportModeActive = (b[5] & 0x80U) != 0;
    telemetry.display.ecoModeActive = (b[6] & 0x80U) != 0;
    telemetry.display.candidateStateLastUpdateMs = millis();
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
    telemetry.vehicleSoftware.ipcValid = true;
    telemetry.vehicleSoftware.ipcMajor = b[5];
    telemetry.vehicleSoftware.ipcMinor = b[6];
    telemetry.vehicleSoftware.ipcPatch = b[7];
    telemetry.vehicleSoftware.ipcLastUpdateMs = millis();
}

static void decode606(const uint8_t *b)
{
    telemetry.vehicleSoftware.vcuValid = true;
    telemetry.vehicleSoftware.vcuMajor = b[1];
    telemetry.vehicleSoftware.vcuMinor = b[0];
    telemetry.vehicleSoftware.vcuBuild = static_cast<uint16_t>(b[4]) |
        (static_cast<uint16_t>(b[5]) << 8);
    telemetry.vehicleSoftware.vcuLastUpdateMs = millis();
}

static void decode0ac(const uint8_t *b)
{
    telemetry.vehicleSoftware.bduValid = true;
    telemetry.vehicleSoftware.bduMajor = b[0];
    telemetry.vehicleSoftware.bduMinor = b[1];
    telemetry.vehicleSoftware.bduPatch = b[2];
    telemetry.vehicleSoftware.bduLastUpdateMs = millis();
}

void decoderDisplayCanHandleFrame(const MotCanFrame &frame)
{
    if (frame.extended) return;
    if (frame.id == 0x0AC && frame.dlc >= 3) {
        decode0ac(frame.data);
        return;
    }
    if (frame.dlc < 8) return;

    switch (frame.id) {
        case 0x601: decode601(frame.data); break;
        case 0x602: decode602(frame.data); break;
        case 0x603:
            decoderDisplayCanHandleLocalStatusFrame(frame);
            decode603(frame.data);
            break;
        // 0x604 data[4] bit 4 is not a plugged flag. Bytes 5..7 are the
        // passively confirmed IPC software-version tuple.
        case 0x604: decode604(frame.data); break;
        case 0x606: decode606(frame.data); break;
        default: break;
    }
}
