#include "telemetry_json.h"
#include <ArduinoJson.h>

String telemetryToJson(const TelemetryState &state)
{
    JsonDocument doc;

    doc["vehicle"]["name"] = state.vehicle.name;
    doc["vehicle"]["profile"] = state.vehicle.profile;

    doc["display"]["valid"] = state.display.valid;
    if (!isnan(state.display.soc)) doc["display"]["soc"] = serialized(String(state.display.soc, 1));
    if (!isnan(state.display.speed)) doc["display"]["speed"] = serialized(String(state.display.speed, 1));
    if (!isnan(state.display.odo)) doc["display"]["odo"] = serialized(String(state.display.odo, 1));
    doc["display"]["range"] = state.display.range;
    doc["display"]["last_update_ms"] = state.display.lastUpdateMs;

    doc["charging"]["valid"] = state.charging.valid;
    doc["charging"]["is_charging"] = state.charging.isCharging;
    doc["charging"]["plugged"] = state.charging.plugged;
    doc["charging"]["plugged_unconfirmed"] = state.charging.pluggedUnconfirmed;
    doc["charging"]["power_display"] = state.charging.powerDisplay;
    doc["charging"]["power_signed"] = state.charging.powerSigned;
    doc["charging"]["last_update_ms"] = state.charging.lastUpdateMs;

    doc["bms"]["valid"] = state.bms.valid;
    if (!isnan(state.bms.packVoltage)) doc["bms"]["pack_voltage"] = serialized(String(state.bms.packVoltage, 2));
    if (!isnan(state.bms.packCurrent)) doc["bms"]["pack_current"] = serialized(String(state.bms.packCurrent, 2));
    if (!isnan(state.bms.packPower)) doc["bms"]["pack_power"] = serialized(String(state.bms.packPower, 2));
    if (!isnan(state.bms.minCellVoltage)) doc["bms"]["min_cell_voltage"] = serialized(String(state.bms.minCellVoltage, 3));
    if (!isnan(state.bms.maxCellVoltage)) doc["bms"]["max_cell_voltage"] = serialized(String(state.bms.maxCellVoltage, 3));
    if (!isnan(state.bms.batteryTemperature)) doc["bms"]["battery_temperature"] = serialized(String(state.bms.batteryTemperature, 1));
    doc["bms"]["last_update_ms"] = state.bms.lastUpdateMs;

    doc["system"]["firmware"] = state.system.firmware;
    doc["system"]["device_id"] = state.system.deviceId;
    doc["system"]["network_mode"] = state.system.networkMode;
    doc["system"]["ip"] = state.system.ip;
    doc["system"]["mqtt_prefix"] = state.system.mqttPrefix;
    doc["system"]["rssi"] = state.system.rssi;
    doc["system"]["uptime"] = state.system.uptime;

    doc["last_can_frame_ms"] = state.lastCanFrameMs;

    String out;
    serializeJson(doc, out);
    return out;
}
