#pragma once

#include "decoders/decoder_profile.h"

enum class C6LocalPeripheralSlot : uint8_t {
    BoardBattery = 0,
    TyreFrontLeft = 1,
    TyreFrontRight = 2,
    TyreRearLeft = 3,
    TyreRearRight = 4,
    Count = 5,
};

struct C6LocalPeripheralBinding {
    String address;
    String protocol;
};

struct C6Configuration {
    DecoderProfile can1Profile = DECODER_PROFILE_STANDARD_CAN_V1_PIONEER;
    DecoderProfile can2Profile = DECODER_PROFILE_DISPLAY_CAN;
    String wifiSsid;
    String wifiPassword;
    String wifi2Ssid;
    String wifi2Password;
    String adminPassword;
    String setupPassword;
    bool motCloudEnabled = true;
    bool abrpEnabled = false;
    bool localMqttEnabled = false;
    bool localMqttUseClientCertificate = false;
    bool localMqttLocationEnabled = false;
    bool localMqttLocationCleanupPending = false;
    bool localMqttLegacyCleanupPending = true;
    String localMqttHost;
    uint16_t localMqttPort = 8883;
    String localMqttVehicleId;
    String localMqttTopicPrefix = "mot";
    String localMqttUsername;
    bool gpsEnabled = true;
    bool offlineCacheEnabled = true;
    String abrpApiKey;
    String abrpUserToken;
    bool onboardingComplete = false;
    uint8_t onboardingStep = 1;
    bool otaEnabled = false;
    uint32_t publishIntervalMs = 5000;
    uint32_t localMqttPublishIntervalMs = 60000;
    uint8_t displayLanguage = 0;
    uint8_t displayUnitSystem = 0;
    uint16_t displayConfigRevision = 0;
    C6LocalPeripheralBinding localPeripherals[
        static_cast<size_t>(C6LocalPeripheralSlot::Count)];
};

extern C6Configuration c6Config;

void c6ConfigLoad();
void c6ConfigSave();
bool c6ConfigCan1ProfileAllowed(DecoderProfile profile);
bool c6ConfigSetCanProfile(size_t channel, DecoderProfile profile);
bool c6ConfigSetWifi(const String &ssid, const String &password);
void c6ConfigClearWifi();
bool c6ConfigSetWifi2(const String &ssid, const String &password);
void c6ConfigClearWifi2();
bool c6ConfigAdminConfigured();
bool c6ConfigSetAdminPassword(const String &password);
bool c6ConfigSetAbrpCredentials(const String &apiKey, const String &userToken);
void c6ConfigClearAbrpCredentials();
void c6ConfigSetMotCloudEnabled(bool enabled);
void c6ConfigSetAbrpEnabled(bool enabled);
void c6ConfigSetLocalMqttEnabled(bool enabled);
bool c6ConfigSetLocalMqttConnection(const String &host, uint16_t port,
    const String &vehicleId, const String &topicPrefix, const String &username,
    bool useClientCertificate);
void c6ConfigSetLocalMqttLocationEnabled(bool enabled);
void c6ConfigMarkLocalMqttLocationCleanupComplete();
void c6ConfigMarkLocalMqttEnvelopeMigrationComplete();
void c6ConfigSetOfflineCacheEnabled(bool enabled);
String c6ConfigRecoverAdminPassword();
String c6ConfigExportJson(bool includeSecrets = false);
bool c6ConfigImportJson(const String &json, String &error);
void c6ConfigFactoryReset();
bool c6ConfigSetDisplayPresentation(uint8_t language, uint8_t unitSystem,
    uint16_t revision);
const C6LocalPeripheralBinding &c6ConfigLocalPeripheral(C6LocalPeripheralSlot slot);
bool c6ConfigSetLocalPeripheral(C6LocalPeripheralSlot slot, const String &address,
    const String &protocol);
void c6ConfigClearLocalPeripheral(C6LocalPeripheralSlot slot);
