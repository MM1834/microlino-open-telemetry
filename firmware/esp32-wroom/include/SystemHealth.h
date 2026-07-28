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
  bool gpsStarted = false;
  bool gpsSeen = false;
  bool gpsValid = false;
  double gpsLatitude = 0.0;
  double gpsLongitude = 0.0;
  uint32_t gpsSatellites = 0;
  double gpsHdop = 0.0;
  uint32_t gpsAgeMs = 0;
  String utc;

  MqttDiagResult mqtt;
};

class SystemHealth {
public:
  static String toJson(const SystemHealthResult& h);
  static String uptimeText(uint32_t seconds);
};
