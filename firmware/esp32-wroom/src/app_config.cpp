#include "app_config.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "config/config_keys.h"
#include "system/device_id.h"
#include "system/version.h"

AppConfig config;
AppConfigurationManager appConfigManager;

bool isValidLocalAdminPassword(const String& value)
{
    String password = value;
    password.trim();
    if (password.length() < 12 || password.length() > 63) return false;
    for (size_t i = 0; i < password.length(); ++i) {
        const uint8_t character = static_cast<uint8_t>(password[i]);
        if (character < 32 || character > 126) return false;
    }
    return true;
}

namespace {
Preferences prefs;
constexpr char PREFERENCES_NAMESPACE[] = "mot";

void setStringIfPresent(const JsonDocument& doc, const char* key, String& target)
{
    if (!doc[key].isNull()) target = doc[key].as<String>();
}
}

void AppConfigurationManager::normalize()
{
    // Migration from older builds where mqttPrefix contained the full base topic,
    // for example "mot/microlino". New format is prefix="mot" + vehicleId.
    config.mqttPrefix.trim();
    config.vehicleId.trim();
    if (config.vehicleId.isEmpty()) {
        if (config.mqttPrefix.startsWith("mot/") && config.mqttPrefix.length() > 4) {
            config.vehicleId = config.mqttPrefix.substring(4);
            config.mqttPrefix = "mot";
        } else {
            config.vehicleId = "pioneer";
        }
    }

    config.deviceName = normalizeIdentifier(config.deviceName, motHostname());
    config.vehicleId = normalizeIdentifier(config.vehicleId, "pioneer");
    config.mqttPrefix = normalizeTopicPrefix(config.mqttPrefix);
    config.mqttPort = normalizePort(config.mqttPort);
    config.publishIntervalMs = normalizePublishInterval(config.publishIntervalMs);
    config.can1Profile = decoderProfileNormalize(config.can1Profile);
    config.can2Profile = decoderProfileNormalize(config.can2Profile, DECODER_PROFILE_DISABLED);
}

void AppConfigurationManager::load()
{
    prefs.begin(PREFERENCES_NAMESPACE, true);
    config.vehicleName = prefs.getString("vehicle", "Microlino Pioneer");
    config.deviceName = prefs.getString("deviceName", "");
    config.vehicleId = prefs.getString("vehicleId", "");
    config.mqttPrefix = prefs.getString("prefix", "mot");
    config.wifiSsid = prefs.getString("ssid", "");
    config.wifiPass = prefs.getString("pass", "");
    config.mqttHost = prefs.getString("mqttHost", "");
    config.mqttPort = prefs.getUShort("mqttPort", 1883);
    config.mqttUser = prefs.getString("mqttUser", "");
    config.mqttPass = prefs.getString("mqttPass", "");
    config.mqttServiceEnabled = prefs.isKey("svcMqtt") ? prefs.getBool("svcMqtt", false) : !config.mqttHost.isEmpty();
    config.awsServiceEnabled = prefs.isKey("svcAws") ? prefs.getBool("svcAws", true) : true;
    config.abrpApiKey = prefs.getString("abrpKey", "");
    config.abrpUserToken = prefs.getString("abrpToken", "");
    config.abrpServiceEnabled = prefs.isKey("svcAbrp") ? prefs.getBool("svcAbrp", false) : (!config.abrpApiKey.isEmpty() && !config.abrpUserToken.isEmpty());
    config.gpsEnabled = prefs.getBool("gpsEn", true);
    config.can1Profile = decoderProfileNormalize(prefs.getUChar("can1", DECODER_PROFILE_DISPLAY_CAN));
    config.can2Profile = decoderProfileNormalize(prefs.getUChar("can2", DECODER_PROFILE_DISABLED), DECODER_PROFILE_DISABLED);
    config.otaEnabled = prefs.getBool("otaEn", false);
    config.otaPassword = prefs.getString("otaPass", "");
    config.publishIntervalMs = prefs.getUInt("pubMs", 5000);
    config.onboardingComplete = prefs.getBool("onboarded", false);
    prefs.end();

    normalize();
}

void AppConfigurationManager::save()
{
    normalize();

    prefs.begin(PREFERENCES_NAMESPACE, false);
    prefs.putString("vehicle", config.vehicleName);
    prefs.putString("deviceName", config.deviceName);
    prefs.putString("vehicleId", config.vehicleId);
    prefs.putString("prefix", config.mqttPrefix);
    prefs.putString("ssid", config.wifiSsid);
    prefs.putString("pass", config.wifiPass);
    prefs.putString("mqttHost", config.mqttHost);
    prefs.putUShort("mqttPort", config.mqttPort);
    prefs.putString("mqttUser", config.mqttUser);
    prefs.putString("mqttPass", config.mqttPass);
    prefs.putBool("svcMqtt", config.mqttServiceEnabled);
    prefs.putBool("svcAws", config.awsServiceEnabled);
    prefs.putString("abrpKey", config.abrpApiKey);
    prefs.putString("abrpToken", config.abrpUserToken);
    prefs.putBool("svcAbrp", config.abrpServiceEnabled);
    prefs.putBool("gpsEn", config.gpsEnabled);
    prefs.putUChar("can1", config.can1Profile);
    prefs.putUChar("can2", config.can2Profile);
    prefs.putBool("otaEn", config.otaEnabled);
    prefs.putString("otaPass", config.otaPassword);
    prefs.putUInt("pubMs", config.publishIntervalMs);
    prefs.putBool("onboarded", config.onboardingComplete);
    prefs.end();
}

void AppConfigurationManager::clear()
{
    prefs.begin(PREFERENCES_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    config = AppConfig();
    normalize();
}

bool AppConfig::mqttEnabled() const
{
    String host = mqttHost;
    host.trim();
    return mqttServiceEnabled && !host.isEmpty();
}

bool AppConfig::awsEnabled() const
{
#ifdef MOT_AWS_IOT
    return awsServiceEnabled;
#else
    return false;
#endif
}

bool AppConfig::abrpEnabled() const
{
    String key = abrpApiKey;
    String token = abrpUserToken;
    key.trim();
    token.trim();
    return abrpServiceEnabled && !key.isEmpty() && !token.isEmpty();
}

bool AppConfig::localAdminConfigured() const
{
    return isValidLocalAdminPassword(otaPassword);
}

String AppConfig::mqttClientId() const
{
    String id = ConfigurationManager::normalizeIdentifier(deviceName, motHostname());
    if (!id.startsWith("mot-")) id = "mot-" + id;
    return id;
}

String AppConfigurationManager::exportJson(bool includeSecrets) const
{
    JsonDocument doc;

    doc[ConfigKeys::SCHEMA_VERSION_KEY] = ConfigKeys::SCHEMA_VERSION;
    doc[ConfigKeys::FIRMWARE] = MOT_VERSION;
    doc[ConfigKeys::BOARD] = MOT_BOARD;
    doc[ConfigKeys::VEHICLE_NAME] = config.vehicleName;
    doc[ConfigKeys::VEHICLE_ID] = config.vehicleId;
    doc[ConfigKeys::DEVICE_NAME] = config.deviceName;
    doc[ConfigKeys::MQTT_PREFIX] = config.mqttPrefix;

    doc[ConfigKeys::WIFI_SSID] = config.wifiSsid;
    if (includeSecrets) doc[ConfigKeys::WIFI_PASS] = config.wifiPass;

    doc[ConfigKeys::SERVICES][ConfigKeys::MQTT_SERVICE] = config.mqttServiceEnabled;
    doc[ConfigKeys::SERVICES][ConfigKeys::AWS_SERVICE] = config.awsServiceEnabled;
    doc[ConfigKeys::SERVICES][ConfigKeys::ABRP_SERVICE] = config.abrpServiceEnabled;
    doc[ConfigKeys::GPS_ENABLED] = config.gpsEnabled;

    doc[ConfigKeys::MQTT_HOST] = config.mqttHost;
    doc[ConfigKeys::MQTT_PORT] = config.mqttPort;
    doc[ConfigKeys::MQTT_USER] = config.mqttUser;
    if (includeSecrets) doc[ConfigKeys::MQTT_PASS] = config.mqttPass;
    doc[ConfigKeys::PUBLISH_INTERVAL_MS] = config.publishIntervalMs;

    if (includeSecrets) {
        doc[ConfigKeys::ABRP_API_KEY] = config.abrpApiKey;
        doc[ConfigKeys::ABRP_USER_TOKEN] = config.abrpUserToken;
    }

    doc[ConfigKeys::CAN1_PROFILE] = static_cast<int>(config.can1Profile);
    doc[ConfigKeys::CAN2_PROFILE] = static_cast<int>(config.can2Profile);
    doc[ConfigKeys::ONBOARDING_COMPLETE] = config.onboardingComplete;
    doc[ConfigKeys::OTA_ENABLED] = config.otaEnabled;
    if (includeSecrets) doc[ConfigKeys::OTA_PASSWORD] = config.otaPassword;

    String out;
    serializeJsonPretty(doc, out);
    return out;
}

ConfigurationValidationResult AppConfigurationManager::validate() const
{
    ConfigurationValidationResult result;
    if (config.mqttPort == 0) {
        result.valid = false;
        result.error = "mqttPort must be between 1 and 65535";
    } else if (config.publishIntervalMs < 1000) {
        result.valid = false;
        result.error = "publishIntervalMs must be at least 1000";
    }
    return result;
}

bool AppConfigurationManager::importJson(const String& json, String& error)
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

    const AppConfig previous = config;
    AppConfig candidate = config;
    config = candidate;

    setStringIfPresent(doc, ConfigKeys::VEHICLE_NAME, config.vehicleName);
    setStringIfPresent(doc, ConfigKeys::VEHICLE_ID, config.vehicleId);
    setStringIfPresent(doc, ConfigKeys::DEVICE_NAME, config.deviceName);
    setStringIfPresent(doc, ConfigKeys::MQTT_PREFIX, config.mqttPrefix);
    setStringIfPresent(doc, ConfigKeys::WIFI_SSID, config.wifiSsid);
    setStringIfPresent(doc, ConfigKeys::WIFI_PASS, config.wifiPass);

    if (!doc[ConfigKeys::SERVICES][ConfigKeys::MQTT_SERVICE].isNull()) config.mqttServiceEnabled = doc[ConfigKeys::SERVICES][ConfigKeys::MQTT_SERVICE].as<bool>();
    if (!doc[ConfigKeys::SERVICES][ConfigKeys::AWS_SERVICE].isNull()) config.awsServiceEnabled = doc[ConfigKeys::SERVICES][ConfigKeys::AWS_SERVICE].as<bool>();
    if (!doc[ConfigKeys::SERVICES][ConfigKeys::ABRP_SERVICE].isNull()) config.abrpServiceEnabled = doc[ConfigKeys::SERVICES][ConfigKeys::ABRP_SERVICE].as<bool>();
    if (!doc[ConfigKeys::GPS_ENABLED].isNull()) config.gpsEnabled = doc[ConfigKeys::GPS_ENABLED].as<bool>();

    setStringIfPresent(doc, ConfigKeys::MQTT_HOST, config.mqttHost);
    if (!doc[ConfigKeys::MQTT_PORT].isNull()) config.mqttPort = doc[ConfigKeys::MQTT_PORT].as<uint16_t>();
    setStringIfPresent(doc, ConfigKeys::MQTT_USER, config.mqttUser);
    setStringIfPresent(doc, ConfigKeys::MQTT_PASS, config.mqttPass);
    if (!doc[ConfigKeys::PUBLISH_INTERVAL_MS].isNull()) config.publishIntervalMs = doc[ConfigKeys::PUBLISH_INTERVAL_MS].as<uint32_t>();

    setStringIfPresent(doc, ConfigKeys::ABRP_API_KEY, config.abrpApiKey);
    setStringIfPresent(doc, ConfigKeys::ABRP_USER_TOKEN, config.abrpUserToken);

    if (!doc[ConfigKeys::CAN1_PROFILE].isNull()) config.can1Profile = decoderProfileNormalize(doc[ConfigKeys::CAN1_PROFILE].as<int>());
    if (!doc[ConfigKeys::CAN2_PROFILE].isNull()) config.can2Profile = decoderProfileNormalize(doc[ConfigKeys::CAN2_PROFILE].as<int>(), DECODER_PROFILE_DISABLED);
    if (!doc[ConfigKeys::ONBOARDING_COMPLETE].isNull()) config.onboardingComplete = doc[ConfigKeys::ONBOARDING_COMPLETE].as<bool>();
    if (!doc[ConfigKeys::OTA_ENABLED].isNull()) config.otaEnabled = doc[ConfigKeys::OTA_ENABLED].as<bool>();
    setStringIfPresent(doc, ConfigKeys::OTA_PASSWORD, config.otaPassword);

    if (!config.localAdminConfigured()) {
        config = previous;
        error = "local admin password must be 12-63 characters";
        return false;
    }

    normalize();
    const ConfigurationValidationResult validation = validate();
    if (!validation.valid) {
        config = previous;
        error = validation.error;
        return false;
    }

    save();
    return true;
}
