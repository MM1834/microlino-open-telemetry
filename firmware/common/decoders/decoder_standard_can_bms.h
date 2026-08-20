#pragma once

#include "../telemetry/telemetry.h"
#include "../can/can_types.h"

#include <Arduino.h>
#include <math.h>

namespace MotStandardCanBms {

struct DecoderRules {
    float currentScaleA;
    uint16_t minPackVoltageMv;
    uint16_t maxPackVoltageMv;
    float maxChargePowerW;
    float maxDischargePowerW;
    float chargingCurrentThresholdA;
    float driveCurrentThresholdA;
};

inline uint16_t readLe16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

inline void decode18d(const uint8_t *data, const DecoderRules &rules)
{
    const int16_t currentRaw = static_cast<int16_t>(readLe16(&data[1]));
    const uint16_t voltageMv = readLe16(&data[3]);
    const float currentA = currentRaw * rules.currentScaleA;
    const float powerW = currentA * (voltageMv / 1000.0f);
    const bool voltagePlausible =
        voltageMv >= rules.minPackVoltageMv && voltageMv <= rules.maxPackVoltageMv;
    const bool currentPlausible = voltagePlausible &&
        powerW <= rules.maxChargePowerW && powerW >= -rules.maxDischargePowerW;

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
        // A profile's signed current scale normalizes positive current as flow
        // into the pack. Vehicle power then keeps MOT's canonical convention:
        // traction/consumption positive, charge/regeneration negative.
        telemetry.bms.vehiclePowerW = -powerW;
        telemetry.bms.isRegenerating =
            !telemetry.bms.plugged && currentA > rules.driveCurrentThresholdA;
        telemetry.bms.isDischarging =
            currentA < -rules.driveCurrentThresholdA;
        telemetry.bms.packCurrentLastUpdateMs = millis();

        telemetry.charging.valid = true;
        telemetry.charging.plugged = telemetry.bms.plugged;
        telemetry.charging.isCharging =
            telemetry.bms.plugged && currentA > rules.chargingCurrentThresholdA;
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

inline void handleFrame(const MotCanFrame &frame, const DecoderRules &rules)
{
    if (frame.extended || frame.dlc < 8) return;
    if (frame.id == 0x18D) decode18d(frame.data, rules);
    else if (frame.id == 0x4AD) decode4ad(frame.data);
}

} // namespace MotStandardCanBms
