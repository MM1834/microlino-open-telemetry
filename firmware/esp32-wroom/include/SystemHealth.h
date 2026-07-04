#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "MqttDiagnostics.h"

struct SystemHealthResult {
  String deviceId;
  String firmwareVersion;
  String buildDate;
  String ip;
  int32_t rssi = 0;
  uint32_t uptimeSec = 0;

  bool wifiOk = false;
  bool dnsOk = false;
  bool tcpOk = false;
  bool mqttOk = false;
  bool canOk = false;

  MqttDiagResult mqtt;
};

class SystemHealth {
public:
  static String toJson(const SystemHealthResult& h);
  static String uptimeText(uint32_t seconds);
};
