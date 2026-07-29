#include "SystemHealth.h"

static String boolJson(bool v) {
  return v ? "true" : "false";
}

static String numberOrNull(double value, unsigned int decimals) {
  return isnan(value) ? String("null") : String(value, decimals);
}

static String esc(const String& s) {
  String out;
  out.reserve(s.length() + 8);

  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out += c;
    }
  }

  return out;
}

String SystemHealth::uptimeText(uint32_t seconds) {
  const uint32_t days = seconds / 86400UL;
  seconds %= 86400UL;
  const uint32_t hours = seconds / 3600UL;
  seconds %= 3600UL;
  const uint32_t minutes = seconds / 60UL;

  String s;
  if (days > 0) s += String(days) + "d ";
  if (hours > 0 || days > 0) s += String(hours) + "h ";
  s += String(minutes) + "m";
  return s;
}

String SystemHealth::toJson(const SystemHealthResult& h) {
  String json = "{";
  json += "\"deviceId\":\"" + esc(h.deviceId) + "\",";
  json += "\"firmwareVersion\":\"" + esc(h.firmwareVersion) + "\",";
  json += "\"buildDate\":\"" + esc(h.buildDate) + "\",";
  json += "\"ip\":\"" + esc(h.ip) + "\",";
  json += "\"rssi\":" + String(h.rssi) + ",";
  json += "\"uptimeSec\":" + String(h.uptimeSec) + ",";
  json += "\"uptimeText\":\"" + esc(uptimeText(h.uptimeSec)) + "\",";
  json += "\"wifiOk\":" + boolJson(h.wifiOk) + ",";
  json += "\"dnsOk\":" + boolJson(h.dnsOk) + ",";
  json += "\"tcpOk\":" + boolJson(h.tcpOk) + ",";
  json += "\"mqttOk\":" + boolJson(h.mqttOk) + ",";
  json += "\"canOk\":" + boolJson(h.canOk) + ",";
  json += "\"utc\":\"" + esc(h.utc) + "\",";
  json += "\"gps\":{";
  json += "\"started\":" + boolJson(h.gpsStarted) + ",";
  json += "\"seen\":" + boolJson(h.gpsSeen) + ",";
  json += "\"detected\":" + boolJson(h.gpsDetected) + ",";
  json += "\"valid\":" + boolJson(h.gpsValid) + ",";
  json += "\"state\":\"" + esc(h.gpsState) + "\",";
  json += "\"latitude\":" + numberOrNull(h.gpsLatitude, 6) + ",";
  json += "\"longitude\":" + numberOrNull(h.gpsLongitude, 6) + ",";
  json += "\"satellites\":" + String(h.gpsSatellites) + ",";
  json += "\"hdop\":" + numberOrNull(h.gpsHdop, 2) + ",";
  json += "\"ageMs\":" + String(h.gpsAgeMs) + "},";
  json += "\"mqtt\":" + MqttDiagnostics::toJson(h.mqtt);
  json += "}";
  return json;
}
