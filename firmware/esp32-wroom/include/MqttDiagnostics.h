#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

struct MqttDiagResult {
  String host;
  uint16_t port = 1883;
  String resolvedIp;

  bool wifiConnected = false;
  bool dnsOk = false;
  bool tcpOk = false;
  bool mqttOk = false;

  int mqttState = 0;
  String message;
  uint32_t durationMs = 0;
};

class MqttDiagnostics {
public:
  static MqttDiagResult test(
    const String& host,
    uint16_t port,
    const String& username,
    const String& password,
    const String& clientIdPrefix = "mot-diag",
    uint32_t tcpTimeoutMs = 5000
  );

  static String toJson(const MqttDiagResult& r);
  static String mqttStateText(int state);
};
