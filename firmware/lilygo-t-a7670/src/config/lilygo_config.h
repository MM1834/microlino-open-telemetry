#pragma once
#include <Arduino.h>
#include "decoders/decoder_profile.h"
struct LilygoConfig {
 String wifiSsid; String wifiPass; bool mqttServiceEnabled = false; bool awsServiceEnabled = true; String lteApn = "gprs.swisscom.ch"; String lteUser; String ltePass; String mqttHost; uint16_t mqttPort = 1883; String mqttUser; String mqttPass; String deviceName; String vehicleId = "pioneer"; String mqttPrefix = "mot"; bool otaEnabled = true; String otaPassword; bool abrpEnabled = false; DecoderProfile canProfile = DECODER_PROFILE_DISPLAY_CAN; bool onboardingComplete = false;
String abrpApiKey;
String abrpUserToken;
};
extern LilygoConfig config;
void loadLilygoConfig(); void saveLilygoConfig(); void clearLilygoConfig();
String lilygoConfigJson(bool includeSecrets = false); String lilygoDeviceName();
