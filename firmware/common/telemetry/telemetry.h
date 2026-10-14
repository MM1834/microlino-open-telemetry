#pragma once

#include <Arduino.h>

struct MotDisplayTelemetry {
    bool valid = false;
    float soc = NAN;
    float speedKmh = NAN;
    float odometerKm = NAN;
    float tripKm = NAN;
    int estimatedRangeKm = -1;
    bool gearStateValid = false;
    uint8_t gearState = 0;
    bool lightStateValid = false;
    bool positionLightActive = false;
    bool lowBeamActive = false;
    bool driveModeStateValid = false;
    bool sportModeActive = false;
    bool ecoModeActive = false;
    bool indicatorStateValid = false;
    bool leftIndicatorActive = false;
    bool rightIndicatorActive = false;
    bool safetyStateValid = false;
    bool parkingBrakeActive = false;
    uint8_t warning90Code = 0;
    bool warningHblAhActive = false;
    bool drivetrainWarningYellow = false;
    uint32_t lastUpdateMs = 0;
    uint32_t indicatorLastUpdateMs = 0;
    uint32_t safetyLastUpdateMs = 0;
    uint32_t candidateStateLastUpdateMs = 0;
};

struct MotChargingTelemetry {
    bool valid = false;
    bool isCharging = false;
    bool plugged = false;
    uint8_t powerDisplay = 0;
    int powerSigned = 0;
    uint32_t lastUpdateMs = 0;
};

struct MotAccessTelemetry {
    bool centralLockValid = false;
    bool centralLocked = false;
    bool doorOpenValid = false;
    bool doorOpen = false;
    bool lockSuppressedByCharging = false;
    bool lockRawSeen = false;
    uint8_t lockRaw = 0;
    uint32_t lastUpdateMs = 0;
};

struct MotBmsTelemetry {
    bool packStatusValid = false;
    bool packCurrentValid = false;
    bool cellVoltagesValid = false;
    bool standardSocValid = false;
    bool pioneerSocValid = false;
    bool sohValid = false;
    uint32_t packVoltageMv = 0;
    int16_t packCurrentRaw = 0;
    float packCurrentA = NAN;
    float packPowerW = NAN;
    float vehiclePowerW = NAN;
    bool isRegenerating = false;
    bool isDischarging = false;
    float socPercent = NAN;
    float socInternal = NAN;
    float socDisplay = NAN;
    float sohPercent = NAN;
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
    uint32_t pioneerSocLastUpdateMs = 0;
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
    MotAccessTelemetry access;
    MotBmsTelemetry bms;
    MotSystemTelemetry system;
};

extern MotTelemetry telemetry;

void telemetryInit();
void telemetryUpdateSystemRuntime();
bool telemetryIsCharging();
