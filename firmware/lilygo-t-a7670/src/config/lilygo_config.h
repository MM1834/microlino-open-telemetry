#pragma once

#include <Arduino.h>

#include "config/configuration_manager.h"
#include "decoders/decoder_profile.h"

struct LilygoConfig {
    String wifiSsid;
    String wifiPass;
    bool mqttServiceEnabled = false;
    bool awsServiceEnabled = true;
    String lteApn;
    String lteUser;
    String ltePass;
    String mqttHost;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPass;
    String deviceName;
    String vehicleId = "pioneer-lilygo";
    String mqttPrefix = "mot";
    bool otaEnabled = false;
    String otaPassword;
    bool abrpEnabled = false;
    bool gpsEnabled = true;
    bool offlineCacheEnabled = false;
    DecoderProfile canProfile = DECODER_PROFILE_STANDARD_CAN_V1_PIONEER;
    DecoderProfile can2Profile = DECODER_PROFILE_DISPLAY_CAN;
    bool onboardingComplete = false;
    String abrpApiKey;
    String abrpUserToken;

    bool localAdminConfigured() const;
    static bool validLocalAdminPassword(const String& password);
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
