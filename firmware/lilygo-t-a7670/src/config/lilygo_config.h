#pragma once

#include <Arduino.h>

#include "config/configuration_manager.h"
#include "decoders/decoder_profile.h"

struct LilygoConfig {
    String wifiSsid;
    String wifiPass;
    bool mqttServiceEnabled = false;
    bool awsServiceEnabled = true;
    String lteApn = "gprs.swisscom.ch";
    String lteUser;
    String ltePass;
    String mqttHost;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPass;
    String deviceName;
    String vehicleId = "pioneer";
    String mqttPrefix = "mot";
    bool otaEnabled = true;
    String otaPassword;
    bool abrpEnabled = false;
    DecoderProfile canProfile = DECODER_PROFILE_DISPLAY_CAN;
    bool onboardingComplete = false;
    String abrpApiKey;
    String abrpUserToken;
};

class LilygoConfigurationManager final : public ConfigurationManager {
public:
    void load() override;
    void save() override;
    void clear() override;
    String exportJson(bool includeSecrets) const override;
    bool importJson(const String& json, String& error) override;
    ConfigurationValidationResult validate() const override;

    void normalize();
};

extern LilygoConfig config;
extern LilygoConfigurationManager lilygoConfigManager;

String lilygoDeviceName();
