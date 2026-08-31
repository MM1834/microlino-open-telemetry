#include "telemetry_json.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>

String telemetryToJson(const MotTelemetry &state)
{
    JsonDocument doc;

    doc["display"]["valid"] = state.display.valid;
    if (!isnan(state.display.soc)) doc["display"]["soc"] = serialized(String(state.display.soc, 1));
    if (!isnan(state.display.speedKmh)) doc["display"]["speed_kmh"] = serialized(String(state.display.speedKmh, 1));
    if (!isnan(state.display.odometerKm)) doc["display"]["odometer_km"] = serialized(String(state.display.odometerKm, 1));
    doc["display"]["estimated_range_km"] = state.display.estimatedRangeKm;
    doc["display"]["last_update_ms"] = state.display.lastUpdateMs;

    doc["charging"]["valid"] = state.charging.valid;
    doc["charging"]["is_charging"] = state.charging.isCharging;
    doc["charging"]["plugged"] = state.charging.plugged;
    doc["charging"]["power_display"] = state.charging.powerDisplay;
    doc["charging"]["power_signed"] = state.charging.powerSigned;
    doc["charging"]["last_update_ms"] = state.charging.lastUpdateMs;

    doc["bms"]["pack_status_valid"] = state.bms.packStatusValid;
    doc["bms"]["pack_current_valid"] = state.bms.packCurrentValid;
    doc["bms"]["cell_voltages_valid"] = state.bms.cellVoltagesValid;
    if (state.bms.packStatusValid) {
        doc["bms"]["pack_voltage_v"] = serialized(String(state.bms.packVoltageMv / 1000.0f, 3));
    }
    if (state.bms.standardSocValid)
        doc["bms"]["standard_soc"] = serialized(String(state.bms.socPercent, 2));
    if (state.bms.sohValid)
        doc["bms"]["soh_percent"] = serialized(String(state.bms.sohPercent, 2));
    if (state.bms.packCurrentValid) {
        doc["bms"]["pack_current_a"] = serialized(String(state.bms.packCurrentA, 1));
        doc["bms"]["pack_power_w"] = serialized(String(state.bms.packPowerW, 0));
        doc["bms"]["vehicle_power_w"] = serialized(String(state.bms.vehiclePowerW, 0));
        doc["bms"]["is_regenerating"] = state.bms.isRegenerating;
        doc["bms"]["is_discharging"] = state.bms.isDischarging;
    }
    doc["bms"]["status_byte"] = state.bms.statusByte;
    doc["bms"]["plugged"] = state.bms.plugged;
    if (state.bms.cellVoltagesValid) {
        doc["bms"]["cell_a_mv"] = state.bms.cellVoltageAMv;
        doc["bms"]["cell_b_mv"] = state.bms.cellVoltageBMv;
        doc["bms"]["cell_min_mv"] = state.bms.minCellVoltageMv;
        doc["bms"]["cell_max_mv"] = state.bms.maxCellVoltageMv;
        doc["bms"]["cell_delta_mv"] = state.bms.cellVoltageDeltaMv;
    }

    doc["system"]["firmware_version"] = state.system.firmwareVersion;
    doc["system"]["device_id"] = state.system.deviceId;
    doc["system"]["network_mode"] = state.system.networkMode;
    doc["system"]["ip_address"] = state.system.ipAddress;
    doc["system"]["wifi_rssi"] = state.system.wifiRssi;
    doc["system"]["uptime_sec"] = state.system.uptimeSec;

    String out;
    serializeJson(doc, out);
    return out;
}
