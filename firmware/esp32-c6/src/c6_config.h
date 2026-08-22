#pragma once

#include "decoders/decoder_profile.h"

struct C6Configuration {
    DecoderProfile can1Profile = DECODER_PROFILE_STANDARD_CAN_V1_PIONEER;
    DecoderProfile can2Profile = DECODER_PROFILE_DISPLAY_CAN;
    String wifiSsid;
    String wifiPassword;
    String wifi2Ssid;
    String wifi2Password;
    String adminPassword;
    String setupPassword;
    bool abrpEnabled = false;
    bool gpsEnabled = true;
    bool offlineCacheEnabled = false;
    String abrpApiKey;
    String abrpUserToken;
    bool onboardingComplete = false;
    bool otaEnabled = false;
    uint32_t publishIntervalMs = 5000;
};

extern C6Configuration c6Config;

void c6ConfigLoad();
void c6ConfigSave();
bool c6ConfigSetCanProfile(size_t channel, DecoderProfile profile);
bool c6ConfigSetWifi(const String &ssid, const String &password);
void c6ConfigClearWifi();
bool c6ConfigSetWifi2(const String &ssid, const String &password);
void c6ConfigClearWifi2();
bool c6ConfigAdminConfigured();
bool c6ConfigSetAdminPassword(const String &password);
bool c6ConfigSetAbrpCredentials(const String &apiKey, const String &userToken);
void c6ConfigSetAbrpEnabled(bool enabled);
void c6ConfigSetOfflineCacheEnabled(bool enabled);
String c6ConfigRecoverAdminPassword();
String c6ConfigExportJson(bool includeSecrets = false);
bool c6ConfigImportJson(const String &json, String &error);
void c6ConfigFactoryReset();
