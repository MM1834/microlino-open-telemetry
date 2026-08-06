#pragma once

#include <Arduino.h>

struct MotDisplayTelemetry {
    bool valid = false;
    float soc = NAN;
    float speedKmh = NAN;
    float odometerKm = NAN;
    int estimatedRangeKm = -1;
    uint32_t lastUpdateMs = 0;
};

struct MotChargingTelemetry {
    bool valid = false;
    bool isCharging = false;
    bool plugged = false;
    uint8_t powerDisplay = 0;
    int powerSigned = 0;
    uint32_t lastUpdateMs = 0;
};

struct MotBmsTelemetry {
    bool packStatusValid = false;
    bool packCurrentValid = false;
    bool cellVoltagesValid = false;
    uint32_t packVoltageMv = 0;
    int16_t packCurrentRaw = 0;
    float packCurrentA = NAN;
    float packPowerW = NAN;
    float vehiclePowerW = NAN;
    bool isRegenerating = false;
    bool isDischarging = false;
    uint8_t socPercent = 0;
    uint8_t statusByte = 0;
    bool plugged = false;
    uint16_t cellVoltageAMv = 0;
    uint16_t cellVoltageBMv = 0;
    uint16_t minCellVoltageMv = 0;
    uint16_t maxCellVoltageMv = 0;
    uint16_t cellVoltageDeltaMv = 0;
    uint32_t packStatusLastUpdateMs = 0;
    uint32_t packCurrentLastUpdateMs = 0;
    uint32_t statusLastUpdateMs = 0;
    uint32_t cellVoltagesLastUpdateMs = 0;
    uint32_t rejectedPackSamples = 0;
    uint32_t rejectedCurrentSamples = 0;
    uint32_t rejectedCellSamples = 0;
};

struct MotSystemTelemetry {
    String firmwareVersion;
    String deviceId;
    String networkMode;
    String ipAddress;
    int wifiRssi = 0;
    uint32_t uptimeSec = 0;
};

struct MotTelemetry {
    MotDisplayTelemetry display;
    MotChargingTelemetry charging;
    MotBmsTelemetry bms;
    MotSystemTelemetry system;
};

extern MotTelemetry telemetry;

void telemetryInit();
void telemetryUpdateSystemRuntime();
