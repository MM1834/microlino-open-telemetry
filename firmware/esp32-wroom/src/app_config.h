#pragma once

#include <Arduino.h>

enum DecoderProfile {
    DECODER_DISPLAY_CAN = 0,
    DECODER_BMS_1 = 1,
    DECODER_BMS_2 = 2
};

struct AppConfig {
    String vehicleName = "Microlino Pioneer";   // Display name only
    String vehicleId = "pioneer";               // Stable MQTT topic id
    String mqttPrefix = "mot";                  // MQTT namespace/prefix

    String wifiSsid;
    String wifiPass;

    String mqttHost;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPass;

    String abrpApiKey;
    String abrpUserToken;

    DecoderProfile can1Profile = DECODER_DISPLAY_CAN;
    DecoderProfile can2Profile = DECODER_BMS_1;

    bool otaEnabled = false;
    String otaPassword;

    uint32_t publishIntervalMs = 5000;
};

extern AppConfig config;

void loadConfig();
void saveConfig();
void clearConfig();
const char *decoderProfileName(DecoderProfile profile);
