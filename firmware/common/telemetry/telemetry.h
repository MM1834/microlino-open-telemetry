#pragma once

#include <Arduino.h>
#include <math.h>

struct MotDisplayTelemetry {
    bool valid = false;
    float soc = NAN;        // percent
    float speed = NAN;      // km/h
    float odo = NAN;        // km
    int range = -1;         // km, estimated
    uint32_t lastUpdateMs = 0;
};

struct MotChargingTelemetry {
    bool valid = false;
    bool isCharging = false;
    bool plugged = false;           // confirmed when available
    bool pluggedUnconfirmed = false;
    int powerDisplay = 0;           // raw display power value
    int powerSigned = 0;            // positive drive/charge, negative regen candidate
    uint32_t lastUpdateMs = 0;
};

struct MotVehicleTelemetry {
    String name = "Microlino";
    String profile = "Microlino Display CAN";
};

struct MotSystemTelemetry {
    String firmware = "unknown";
    String deviceId = "unknown";
    String networkMode = "unknown";
    String ip = "";
    String mqttPrefix = "mot/default";
    int rssi = 0;
    uint32_t uptime = 0;
};

struct MotBmsTelemetry {
    bool valid = false;
    float packVoltage = NAN;
    float packCurrent = NAN;
    float packPower = NAN;
    float minCellVoltage = NAN;
    float maxCellVoltage = NAN;
    float batteryTemperature = NAN;
    uint32_t lastUpdateMs = 0;
};

struct TelemetryState {
    MotVehicleTelemetry vehicle;
    MotDisplayTelemetry display;
    MotChargingTelemetry charging;
    MotBmsTelemetry bms;
    MotSystemTelemetry system;

    uint32_t lastCanFrameMs = 0;
};

extern TelemetryState telemetry;

void telemetryReset();
bool telemetryHasVehicleData();
