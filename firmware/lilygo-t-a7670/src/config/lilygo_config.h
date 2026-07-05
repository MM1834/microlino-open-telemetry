#pragma once
#include <Arduino.h>
struct LilygoConfig {
 String wifiSsid; String wifiPass; String lteApn = "gprs.swisscom.ch"; String lteUser; String ltePass; String mqttHost; uint16_t mqttPort = 1883; String mqttUser; String mqttPass; String deviceName; String vehicleId = "pioneer"; String mqttPrefix = "mot"; bool otaEnabled = true; String otaPassword;
};
extern LilygoConfig config;
void loadLilygoConfig(); void saveLilygoConfig(); void clearLilygoConfig();
String lilygoConfigJson(bool includeSecrets = false); String lilygoDeviceName();
