#include "lilygo_config.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "config/config_keys.h"
#include "system/version.h"

LilygoConfig config;
LilygoConfigurationManager lilygoConfigManager;

namespace {
Preferences prefs;
constexpr char PREFERENCES_NAMESPACE[] = "mot-lg";

String chipSuffix()
{
    const uint64_t mac = ESP.getEfuseMac();
    char buf[16];
    snprintf(buf, sizeof(buf), "%06X", static_cast<uint32_t>(mac & 0xFFFFFF));
    return String(buf);
}

String getStringOrDefault(const char* key, const char* fallback)
{
    if (!prefs.isKey(key)) return String(fallback);
    return prefs.getString(key, fallback);
}

void setStringIfPresent(const JsonDocument& doc, const char* key, String& target)
{
    if (!doc[key].isNull()) target = doc[key].as<String>();
}
}

String lilygoDeviceName()
{
    return ConfigurationManager::normalizeIdentifier(config.deviceName, String(LILYGO_DEFAULT_DEVICE_PREFIX) + chipSuffix());
}

bool LilygoConfig::validLocalAdminPassword(const String& password)
{
    if (password.length() < 12 || password.length() > 63) return false;
    for (size_t i = 0; i < password.length(); ++i) {
        const uint8_t character = static_cast<uint8_t>(password[i]);
        if (character < 32 || character > 126) return false;
    }
    return true;
}

bool LilygoConfig::localAdminConfigured() const
{
    return validLocalAdminPassword(otaPassword);
}

void LilygoConfigurationManager::normalize()
{
    config.deviceName = normalizeIdentifier(config.deviceName, String(LILYGO_DEFAULT_DEVICE_PREFIX) + chipSuffix());
    config.vehicleId = normalizeIdentifier(config.vehicleId, LILYGO_DEFAULT_VEHICLE_ID);
    config.mqttPrefix = normalizeTopicPrefix(config.mqttPrefix);
    config.mqttPort = normalizePort(config.mqttPort);
    config.lteApn.trim();
    config.lteUser.trim();
    config.canProfile = decoderProfileNormalize(
        config.canProfile, DECODER_PROFILE_STANDARD_CAN_V1_PIONEER);
    config.can2Profile = decoderProfileNormalize(config.can2Profile, DECODER_PROFILE_DISPLAY_CAN);
}

void LilygoConfigurationManager::load()
{
    prefs.begin(PREFERENCES_NAMESPACE, false);
    config.wifiSsid = getStringOrDefault("wifiSsid", "");
    config.wifiPass = getStringOrDefault("wifiPass", "");
    config.lteApn = getStringOrDefault("lteApn", "");
    config.lteUser = getStringOrDefault("lteUser", "");
    config.ltePass = getStringOrDefault("ltePass", "");
    config.mqttHost = getStringOrDefault("mqttHost", "");
    config.mqttPort = prefs.isKey("mqttPort") ? prefs.getUShort("mqttPort", 1883) : 1883;
    config.mqttUser = getStringOrDefault("mqttUser", "");
    config.mqttPass = getStringOrDefault("mqttPass", "");
    config.mqttServiceEnabled = prefs.isKey("svcMqtt") ? prefs.getBool("svcMqtt", false) : !config.mqttHost.isEmpty();
    config.awsServiceEnabled = prefs.isKey("svcAws") ? prefs.getBool("svcAws", true) : true;
    config.deviceName = getStringOrDefault("deviceName", "");
    config.vehicleId = getStringOrDefault("vehicleId", LILYGO_DEFAULT_VEHICLE_ID);
    config.mqttPrefix = getStringOrDefault("mqttPrefix", "mot");
    const bool securityV1 = prefs.getBool("securityV1", false);
    config.otaEnabled = securityV1 && prefs.getBool("otaEnabled", false);
    if (!securityV1) {
        // Existing installations inherited an unsafe OTA-on default. Force one
        // explicit opt-in after installing the hardened firmware.
        prefs.putBool("otaEnabled", false);
        prefs.putBool("securityV1", true);
    }
    config.otaPassword = getStringOrDefault("otaPassword", "");
    config.abrpEnabled = prefs.isKey("abrpEnabled") ? prefs.getBool("abrpEnabled", false) : false;
    config.gpsEnabled = prefs.getBool("gpsEn", true);
    config.offlineCacheEnabled = prefs.getBool("cacheEn", false);
    config.abrpApiKey = getStringOrDefault("abrpApiKey", "");
    config.abrpUserToken = getStringOrDefault("abrpUserToken", "");
    config.onboardingComplete = prefs.getBool("onboarded", false);
    const bool vehicleIdentityV1 = prefs.getBool("lgVehicleV1", false);
    if (!vehicleIdentityV1) {
        if (config.vehicleId == "pioneer") config.vehicleId = "pioneer-lilygo";
        prefs.putString("vehicleId", config.vehicleId);
        prefs.putBool("lgVehicleV1", true);
    }
    const bool dualCanMapV1 = prefs.getBool("dualCanMapV1", false);
    if (!dualCanMapV1) {
        // LG-CAN2-001 changes the physical mapping to match the C6 pilot:
        // native TWAI CAN1 = Standard CAN, MCP2515 CAN2 = Display CAN.
        config.canProfile = DECODER_PROFILE_STANDARD_CAN_V1_PIONEER;
        config.can2Profile = DECODER_PROFILE_DISPLAY_CAN;
        prefs.putUChar("canProfile", static_cast<uint8_t>(config.canProfile));
        prefs.putUChar("can2Profile", static_cast<uint8_t>(config.can2Profile));
        prefs.putBool("dualCanMapV1", true);
    } else {
        config.canProfile = decoderProfileNormalize(
            prefs.getUChar("canProfile", DECODER_PROFILE_STANDARD_CAN_V1_PIONEER),
            DECODER_PROFILE_STANDARD_CAN_V1_PIONEER);
        config.can2Profile = decoderProfileNormalize(
            prefs.getUChar("can2Profile", DECODER_PROFILE_DISPLAY_CAN),
            DECODER_PROFILE_DISPLAY_CAN);
    }
    prefs.end();

    normalize();
}

void LilygoConfigurationManager::save()
{
    normalize();

    prefs.begin(PREFERENCES_NAMESPACE, false);
    prefs.putString("wifiSsid", config.wifiSsid);
    prefs.putString("wifiPass", config.wifiPass);
    prefs.putString("lteApn", config.lteApn);
    prefs.putString("lteUser", config.lteUser);
    prefs.putString("ltePass", config.ltePass);
    prefs.putString("mqttHost", config.mqttHost);
    prefs.putUShort("mqttPort", config.mqttPort);
    prefs.putString("mqttUser", config.mqttUser);
    prefs.putString("mqttPass", config.mqttPass);
    prefs.putBool("svcMqtt", config.mqttServiceEnabled);
    prefs.putBool("svcAws", config.awsServiceEnabled);
    prefs.putString("deviceName", config.deviceName);
    prefs.putString("vehicleId", config.vehicleId);
    prefs.putString("mqttPrefix", config.mqttPrefix);
    prefs.putBool("otaEnabled", config.otaEnabled);
    prefs.putString("otaPassword", config.otaPassword);
    prefs.putBool("abrpEnabled", config.abrpEnabled);
    prefs.putBool("gpsEn", config.gpsEnabled);
    prefs.putBool("cacheEn", config.offlineCacheEnabled);
    prefs.putString("abrpApiKey", config.abrpApiKey);
    prefs.putString("abrpUserToken", config.abrpUserToken);
    prefs.putUChar("canProfile", static_cast<uint8_t>(config.canProfile));
    prefs.putUChar("can2Profile", static_cast<uint8_t>(config.can2Profile));
    prefs.putBool("onboarded", config.onboardingComplete);
    prefs.end();
}

void LilygoConfigurationManager::clear()
{
    prefs.begin(PREFERENCES_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    config = LilygoConfig();
    normalize();
}

String LilygoConfigurationManager::exportJson(bool includeSecrets) const
{
    JsonDocument doc;
    doc[ConfigKeys::SCHEMA_VERSION_KEY] = ConfigKeys::SCHEMA_VERSION;
    doc[ConfigKeys::FIRMWARE] = MOT_VERSION;
    doc[ConfigKeys::BOARD] = MOT_BOARD;
    doc[ConfigKeys::DEVICE_NAME] = lilygoDeviceName();
    doc[ConfigKeys::VEHICLE_ID] = config.vehicleId;
    doc[ConfigKeys::MQTT_PREFIX] = config.mqttPrefix;
    doc[ConfigKeys::WIFI_SSID] = config.wifiSsid;
    doc[ConfigKeys::LTE_APN] = config.lteApn;
    doc[ConfigKeys::SERVICES][ConfigKeys::MQTT_SERVICE] = config.mqttServiceEnabled;
    doc[ConfigKeys::SERVICES][ConfigKeys::AWS_SERVICE] = config.awsServiceEnabled;
    doc[ConfigKeys::SERVICES][ConfigKeys::ABRP_SERVICE] = config.abrpEnabled;
    doc[ConfigKeys::GPS_ENABLED] = config.gpsEnabled;
    doc["offlineCacheEnabled"] = config.offlineCacheEnabled;
    doc[ConfigKeys::MQTT_HOST] = config.mqttHost;
    doc[ConfigKeys::MQTT_PORT] = config.mqttPort;
    doc[ConfigKeys::OTA_ENABLED] = config.otaEnabled;
    doc[ConfigKeys::CAN1_PROFILE] = static_cast<int>(config.canProfile);
    doc[ConfigKeys::CAN2_PROFILE] = static_cast<int>(config.can2Profile);
    doc[ConfigKeys::ONBOARDING_COMPLETE] = config.onboardingComplete;

    if (includeSecrets) {
        doc[ConfigKeys::WIFI_PASS] = config.wifiPass;
        doc[ConfigKeys::LTE_USER] = config.lteUser;
        doc[ConfigKeys::LTE_PASS] = config.ltePass;
        doc[ConfigKeys::MQTT_USER] = config.mqttUser;
        doc[ConfigKeys::MQTT_PASS] = config.mqttPass;
        doc[ConfigKeys::OTA_PASSWORD] = config.otaPassword;
        doc[ConfigKeys::ABRP_API_KEY] = config.abrpApiKey;
        doc[ConfigKeys::ABRP_USER_TOKEN] = config.abrpUserToken;
    }

    String out;
    serializeJsonPretty(doc, out);
    return out;
}

ConfigurationValidationResult LilygoConfigurationManager::validate() const
{
    ConfigurationValidationResult result;
    if (config.mqttPort == 0) {
        result.valid = false;
        result.error = "mqttPort must be between 1 and 65535";
    }
    if (!config.localAdminConfigured()) {
        result.valid = false;
        result.error = "local admin password must be 12-63 printable ASCII characters";
    }
    return result;
}

bool LilygoConfigurationManager::importJson(const String& json, String& error)
{
    JsonDocument doc;
    const DeserializationError parseError = deserializeJson(doc, json);
    if (parseError) {
        error = parseError.c_str();
        return false;
    }

    if (!doc[ConfigKeys::SCHEMA_VERSION_KEY].isNull() &&
        doc[ConfigKeys::SCHEMA_VERSION_KEY].as<int>() > ConfigKeys::SCHEMA_VERSION) {
        error = "unsupported schemaVersion";
        return false;
    }

    const LilygoConfig previous = config;

    setStringIfPresent(doc, ConfigKeys::DEVICE_NAME, config.deviceName);
    setStringIfPresent(doc, ConfigKeys::VEHICLE_ID, config.vehicleId);
    setStringIfPresent(doc, ConfigKeys::MQTT_PREFIX, config.mqttPrefix);
    setStringIfPresent(doc, ConfigKeys::WIFI_SSID, config.wifiSsid);
    setStringIfPresent(doc, ConfigKeys::WIFI_PASS, config.wifiPass);
    setStringIfPresent(doc, ConfigKeys::LTE_APN, config.lteApn);
    setStringIfPresent(doc, ConfigKeys::LTE_USER, config.lteUser);
    setStringIfPresent(doc, ConfigKeys::LTE_PASS, config.ltePass);

    if (!doc[ConfigKeys::SERVICES][ConfigKeys::MQTT_SERVICE].isNull()) config.mqttServiceEnabled = doc[ConfigKeys::SERVICES][ConfigKeys::MQTT_SERVICE].as<bool>();
    if (!doc[ConfigKeys::SERVICES][ConfigKeys::AWS_SERVICE].isNull()) config.awsServiceEnabled = doc[ConfigKeys::SERVICES][ConfigKeys::AWS_SERVICE].as<bool>();
    if (!doc[ConfigKeys::SERVICES][ConfigKeys::ABRP_SERVICE].isNull()) config.abrpEnabled = doc[ConfigKeys::SERVICES][ConfigKeys::ABRP_SERVICE].as<bool>();
    if (!doc[ConfigKeys::GPS_ENABLED].isNull()) config.gpsEnabled = doc[ConfigKeys::GPS_ENABLED].as<bool>();
    if (!doc["offlineCacheEnabled"].isNull()) config.offlineCacheEnabled = doc["offlineCacheEnabled"].as<bool>();

    setStringIfPresent(doc, ConfigKeys::MQTT_HOST, config.mqttHost);
    if (!doc[ConfigKeys::MQTT_PORT].isNull()) config.mqttPort = doc[ConfigKeys::MQTT_PORT].as<uint16_t>();
    setStringIfPresent(doc, ConfigKeys::MQTT_USER, config.mqttUser);
    setStringIfPresent(doc, ConfigKeys::MQTT_PASS, config.mqttPass);
    if (!doc[ConfigKeys::OTA_ENABLED].isNull()) config.otaEnabled = doc[ConfigKeys::OTA_ENABLED].as<bool>();
    setStringIfPresent(doc, ConfigKeys::OTA_PASSWORD, config.otaPassword);
    setStringIfPresent(doc, ConfigKeys::ABRP_API_KEY, config.abrpApiKey);
    setStringIfPresent(doc, ConfigKeys::ABRP_USER_TOKEN, config.abrpUserToken);

    if (!doc[ConfigKeys::CAN1_PROFILE].isNull()) config.canProfile = decoderProfileNormalize(doc[ConfigKeys::CAN1_PROFILE].as<int>());
    else if (!doc[ConfigKeys::LEGACY_CAN_PROFILE].isNull()) config.canProfile = decoderProfileNormalize(doc[ConfigKeys::LEGACY_CAN_PROFILE].as<int>());
    if (!doc[ConfigKeys::CAN2_PROFILE].isNull()) {
        config.can2Profile = decoderProfileNormalize(
            doc[ConfigKeys::CAN2_PROFILE].as<int>(),
            DECODER_PROFILE_STANDARD_CAN_V1_PIONEER);
    }

    if (!doc[ConfigKeys::ONBOARDING_COMPLETE].isNull()) config.onboardingComplete = doc[ConfigKeys::ONBOARDING_COMPLETE].as<bool>();

    normalize();
    if (!config.localAdminConfigured() && previous.localAdminConfigured()) {
        config.otaPassword = previous.otaPassword;
    }
    const ConfigurationValidationResult validation = validate();
    if (!validation.valid) {
        config = previous;
        error = validation.error;
        return false;
    }

    save();
    return true;
}
