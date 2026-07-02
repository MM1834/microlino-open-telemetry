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
