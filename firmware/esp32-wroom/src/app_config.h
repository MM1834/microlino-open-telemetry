#pragma once

#include <Arduino.h>

#include "common/decoders/decoder_profile.h"

struct AppConfig {
    String vehicleName = "Microlino Pioneer";   // Display name only
    String deviceName;                          // Stable hostname / MQTT client id
    String vehicleId = "pioneer";               // Stable MQTT topic id
    String mqttPrefix = "mot";                  // MQTT namespace/prefix

    String wifiSsid;
    String wifiPass;

    String mqttHost;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPass;
    bool mqttServiceEnabled = false;
    bool awsServiceEnabled = true;

    String abrpApiKey;
    String abrpUserToken;
    bool abrpServiceEnabled = false;

    DecoderProfile can1Profile = DECODER_PROFILE_DISPLAY_CAN;
    DecoderProfile can2Profile = DECODER_PROFILE_DISABLED;

    bool otaEnabled = false;
    String otaPassword;

    uint32_t publishIntervalMs = 5000;

    bool onboardingComplete = false;

    bool mqttEnabled() const;
    bool awsEnabled() const;
    bool abrpEnabled() const;
    String mqttClientId() const;
};

extern AppConfig config;

void loadConfig();
void saveConfig();
void clearConfig();
String configToJson(bool includeSecrets = true);
bool importConfigJson(const String& json, String& error);
