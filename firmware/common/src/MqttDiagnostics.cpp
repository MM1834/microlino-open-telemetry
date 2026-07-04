#include "MqttDiagnostics.h"

MqttDiagResult MqttDiagnostics::test(
  const String& host,
  uint16_t port,
  const String& username,
  const String& password,
  const String& clientIdPrefix,
  uint32_t tcpTimeoutMs
) {
  MqttDiagResult r;
  r.host = host;
  r.port = port;
  r.wifiConnected = WiFi.status() == WL_CONNECTED;
  const uint32_t started = millis();
  String cleanHost = host;
  cleanHost.trim();
  if (cleanHost.isEmpty()) {
    r.message = "MQTT disabled: no host configured";
    r.durationMs = millis() - started;
    return r;
  }

  if (!r.wifiConnected) {
    r.message = "WiFi not connected";
    r.durationMs = millis() - started;
    return r;
  }

  IPAddress ip;
  r.dnsOk = WiFi.hostByName(cleanHost.c_str(), ip);
  if (r.dnsOk) {
    r.resolvedIp = ip.toString();
  } else {
    r.message = "DNS lookup failed";
    r.durationMs = millis() - started;
    return r;
  }

  WiFiClient tcpClient;
  tcpClient.setTimeout(tcpTimeoutMs / 1000);
  r.tcpOk = tcpClient.connect(cleanHost.c_str(), port);
  if (!r.tcpOk) {
    r.message = "TCP connection failed";
    r.durationMs = millis() - started;
    return r;
  }
  tcpClient.stop();

  WiFiClient mqttNet;
  PubSubClient mqtt(mqttNet);
  mqtt.setServer(cleanHost.c_str(), port);

  const String clientId = clientIdPrefix + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  bool ok = false;

  if (username.length() > 0) {
    ok = mqtt.connect(clientId.c_str(), username.c_str(), password.c_str());
  } else {
    ok = mqtt.connect(clientId.c_str());
  }

  r.mqttOk = ok;
  r.mqttState = mqtt.state();

  if (ok) {
    r.message = "MQTT login successful";
    mqtt.disconnect();
  } else {
    r.message = mqttStateText(r.mqttState);
  }

  r.durationMs = millis() - started;
  return r;
}

String MqttDiagnostics::mqttStateText(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT: return "MQTT connection timeout";
    case MQTT_CONNECTION_LOST: return "MQTT connection lost";
    case MQTT_CONNECT_FAILED: return "MQTT TCP connect failed";
    case MQTT_DISCONNECTED: return "MQTT disconnected";
    case MQTT_CONNECTED: return "MQTT connected";
    case MQTT_CONNECT_BAD_PROTOCOL: return "MQTT bad protocol";
    case MQTT_CONNECT_BAD_CLIENT_ID: return "MQTT bad client id";
    case MQTT_CONNECT_UNAVAILABLE: return "MQTT server unavailable";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "MQTT bad credentials";
    case MQTT_CONNECT_UNAUTHORIZED: return "MQTT unauthorized";
    default: return "MQTT error rc=" + String(state);
  }
}

static String boolJson(bool v) { return v ? "true" : "false"; }

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

String MqttDiagnostics::toJson(const MqttDiagResult& r) {
  String json = "{";
  json += "\"host\":\"" + esc(r.host) + "\",";
  json += "\"port\":" + String(r.port) + ",";
  json += "\"resolvedIp\":\"" + esc(r.resolvedIp) + "\",";
  json += "\"wifiConnected\":" + boolJson(r.wifiConnected) + ",";
  json += "\"dnsOk\":" + boolJson(r.dnsOk) + ",";
  json += "\"tcpOk\":" + boolJson(r.tcpOk) + ",";
  json += "\"mqttOk\":" + boolJson(r.mqttOk) + ",";
  json += "\"mqttState\":" + String(r.mqttState) + ",";
  json += "\"message\":\"" + esc(r.message) + "\",";
  json += "\"durationMs\":" + String(r.durationMs);
  json += "}";
  return json;
}
