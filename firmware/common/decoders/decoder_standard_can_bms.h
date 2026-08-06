#pragma once

#include "../telemetry/telemetry.h"
#include "../can/can_types.h"

#include <Arduino.h>
#include <math.h>

namespace MotStandardCanBms {

static constexpr float CURRENT_SCALE_A = 0.3f;
static constexpr uint16_t MIN_PACK_VOLTAGE_MV = 40000;
static constexpr uint16_t MAX_PACK_VOLTAGE_MV = 65000;
static constexpr float MAX_CHARGE_POWER_W = 12000.0f;
static constexpr float MAX_DISCHARGE_POWER_W = 25000.0f;
static constexpr float CHARGING_CURRENT_THRESHOLD_A = 2.0f;
static constexpr float DRIVE_CURRENT_THRESHOLD_A = 2.0f;

inline uint16_t readLe16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

inline void decode18d(const uint8_t *data)
{
    const int16_t currentRaw = static_cast<int16_t>(readLe16(&data[1]));
    const uint16_t voltageMv = readLe16(&data[3]);
    const float currentA = currentRaw * CURRENT_SCALE_A;
    const float powerW = currentA * (voltageMv / 1000.0f);
    const bool voltagePlausible =
        voltageMv >= MIN_PACK_VOLTAGE_MV && voltageMv <= MAX_PACK_VOLTAGE_MV;
    const bool currentPlausible = voltagePlausible &&
        powerW <= MAX_CHARGE_POWER_W && powerW >= -MAX_DISCHARGE_POWER_W;

    telemetry.bms.statusByte = data[6];
    telemetry.bms.plugged = data[6] == 0x20;
    telemetry.bms.statusLastUpdateMs = millis();

    if (voltagePlausible) {
        telemetry.bms.packStatusValid = true;
        telemetry.bms.packVoltageMv = voltageMv;
        telemetry.bms.socPercent = data[7];
        telemetry.bms.packStatusLastUpdateMs = millis();
    } else {
        telemetry.bms.rejectedPackSamples++;
    }

    if (currentPlausible) {
        telemetry.bms.packCurrentValid = true;
        telemetry.bms.packCurrentRaw = currentRaw;
        telemetry.bms.packCurrentA = currentA;
        telemetry.bms.packPowerW = powerW;
        // The Pioneer field uses the battery convention: positive current flows
        // into the pack (charge/regen), negative current discharges the pack.
        // Vehicle power keeps MOT's existing convention: traction/consumption is
        // positive and energy returned to the battery is negative.
        telemetry.bms.vehiclePowerW = -powerW;
        telemetry.bms.isRegenerating =
            !telemetry.bms.plugged && currentA > DRIVE_CURRENT_THRESHOLD_A;
        telemetry.bms.isDischarging =
            currentA < -DRIVE_CURRENT_THRESHOLD_A;
        telemetry.bms.packCurrentLastUpdateMs = millis();

        telemetry.charging.valid = true;
        telemetry.charging.plugged = telemetry.bms.plugged;
        telemetry.charging.isCharging =
            telemetry.bms.plugged && currentA > CHARGING_CURRENT_THRESHOLD_A;
        telemetry.charging.powerSigned =
            static_cast<int>(lroundf(telemetry.bms.vehiclePowerW / 100.0f));
        telemetry.charging.lastUpdateMs = millis();
    } else {
        telemetry.bms.rejectedCurrentSamples++;
    }
}

inline void decode4ad(const uint8_t *data)
{
    const uint16_t cellA = readLe16(&data[0]);
    const uint16_t cellB = readLe16(&data[2]);
    const bool plausible =
        cellA >= 2000 && cellA <= 5000 && cellB >= 2000 && cellB <= 5000;
    if (!plausible) {
        telemetry.bms.rejectedCellSamples++;
        return;
    }

    telemetry.bms.cellVoltagesValid = true;
    telemetry.bms.cellVoltageAMv = cellA;
    telemetry.bms.cellVoltageBMv = cellB;
    telemetry.bms.minCellVoltageMv = min(cellA, cellB);
    telemetry.bms.maxCellVoltageMv = max(cellA, cellB);
    telemetry.bms.cellVoltageDeltaMv =
        telemetry.bms.maxCellVoltageMv - telemetry.bms.minCellVoltageMv;
    telemetry.bms.cellVoltagesLastUpdateMs = millis();
}

inline void handleFrame(const MotCanFrame &frame)
{
    if (frame.extended || frame.dlc < 8) return;
    if (frame.id == 0x18D) decode18d(frame.data);
    else if (frame.id == 0x4AD) decode4ad(frame.data);
}

} // namespace MotStandardCanBms
