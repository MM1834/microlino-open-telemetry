#include "decoder_standard_can_v2.h"
#include "../telemetry/telemetry.h"

#include <Arduino.h>
#include <math.h>

namespace {

uint16_t readBe16(const uint8_t *data)
{
    return (static_cast<uint16_t>(data[0]) << 8) |
           static_cast<uint16_t>(data[1]);
}

void decode1b0(const uint8_t *data)
{
    const uint16_t voltageMv = readBe16(&data[2]);
    const uint16_t minimumCellMv = readBe16(&data[4]);
    const uint16_t maximumCellMv = readBe16(&data[6]);
    const bool packPlausible = voltageMv >= 40000 && voltageMv <= 65000;
    const bool cellsPlausible = minimumCellMv >= 2000 && maximumCellMv <= 5000 &&
        minimumCellMv <= maximumCellMv;

    if (packPlausible) {
        telemetry.bms.packStatusValid = true;
        telemetry.bms.packVoltageMv = voltageMv;
        telemetry.bms.packStatusLastUpdateMs = millis();
    } else {
        telemetry.bms.rejectedPackSamples++;
    }

    if (cellsPlausible) {
        telemetry.bms.cellVoltagesValid = true;
        telemetry.bms.cellVoltageAMv = minimumCellMv;
        telemetry.bms.cellVoltageBMv = maximumCellMv;
        telemetry.bms.minCellVoltageMv = minimumCellMv;
        telemetry.bms.maxCellVoltageMv = maximumCellMv;
        telemetry.bms.cellVoltageDeltaMv = maximumCellMv - minimumCellMv;
        telemetry.bms.cellVoltagesLastUpdateMs = millis();
    } else {
        telemetry.bms.rejectedCellSamples++;
    }
}

void decode1b1(const uint8_t *data)
{
    const uint16_t socHundredths = readBe16(&data[0]);
    const uint16_t sohHundredths = readBe16(&data[2]);
    if (socHundredths <= 10000) {
        telemetry.bms.standardSocValid = true;
        telemetry.bms.socPercent = socHundredths / 100.0f;
    }
    if (sohHundredths <= 10000) {
        telemetry.bms.sohValid = true;
        telemetry.bms.sohPercent = sohHundredths / 100.0f;
    }
}

void decode2ba(const uint8_t *data)
{
    const int16_t currentRaw = static_cast<int16_t>(readBe16(&data[0]));
    const float currentA = currentRaw / 10.0f;
    if (!telemetry.bms.packStatusValid ||
        millis() - telemetry.bms.packStatusLastUpdateMs > 10000) {
        return;
    }

    const float powerW = currentA * (telemetry.bms.packVoltageMv / 1000.0f);
    if (powerW < -30000.0f || powerW > 20000.0f) {
        telemetry.bms.rejectedCurrentSamples++;
        return;
    }

    telemetry.bms.packCurrentValid = true;
    telemetry.bms.packCurrentRaw = currentRaw;
    telemetry.bms.packCurrentA = currentA;
    telemetry.bms.packPowerW = powerW;
    telemetry.bms.vehiclePowerW = -powerW;
    telemetry.bms.isRegenerating = !telemetry.bms.plugged && currentA > 2.0f;
    telemetry.bms.isDischarging = currentA < -2.0f;
    telemetry.bms.packCurrentLastUpdateMs = millis();
    telemetry.charging.powerSigned =
        static_cast<int>(lroundf(telemetry.bms.vehiclePowerW / 100.0f));
}

void decode60d(const uint8_t *data)
{
    // Verified 2026-09-14 on a large-battery V2 vehicle:
    // byte 1 bit 3 is set while the charge cable is connected, independent
    // of whether energy is currently flowing.
    telemetry.bms.plugged = (data[1] & 0x08) != 0;
    telemetry.bms.statusLastUpdateMs = millis();
    telemetry.charging.plugged = telemetry.bms.plugged;
}

void decode610(const uint8_t *data)
{
    // Verified state values on the same vehicle:
    // 0x01 = unplugged, 0x05 = plugged/idle, 0x02 = actively charging.
    const uint8_t state = data[0];
    if (state != 0x01 && state != 0x02 && state != 0x05) return;

    telemetry.bms.statusByte = state;
    telemetry.bms.plugged = state != 0x01;
    telemetry.bms.statusLastUpdateMs = millis();
    telemetry.charging.valid = true;
    telemetry.charging.plugged = telemetry.bms.plugged;
    telemetry.charging.isCharging = state == 0x02;
    telemetry.charging.lastUpdateMs = millis();
}

} // namespace

void decoderStandardCanV2HandleFrame(const MotCanFrame &frame)
{
    if (frame.extended || frame.dlc < 8) return;
    if (frame.id == 0x1B0) decode1b0(frame.data);
    else if (frame.id == 0x1B1) decode1b1(frame.data);
    else if (frame.id == 0x2BA) decode2ba(frame.data);
    else if (frame.id == 0x60D) decode60d(frame.data);
    else if (frame.id == 0x610) decode610(frame.data);
}
