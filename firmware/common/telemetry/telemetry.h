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
    MotSystemTelemetry system;
};

extern MotTelemetry telemetry;

void telemetryInit();
void telemetryUpdateSystemRuntime();
