#include "c6_config.h"

#include <Preferences.h>
#include <ArduinoJson.h>

#include "c6_dual_can.h"
#include "web/local_web_security.h"

namespace {
constexpr char PREFERENCES_NAMESPACE[] = "mot";
Preferences preferences;

bool validAdminPassword(const String &password)
{
    if (password.length() < 12 || password.length() > 63) return false;
    for (size_t i = 0; i < password.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(password[i]);
        if (c < 32 || c > 126) return false;
    }
    return true;
}

}

C6Configuration c6Config;

void c6ConfigLoad()
{
    preferences.begin(PREFERENCES_NAMESPACE, true);
    c6Config.can1Profile = decoderProfileNormalize(
        preferences.getUChar("can1", DECODER_PROFILE_STANDARD_CAN_V1_PIONEER),
        DECODER_PROFILE_STANDARD_CAN_V1_PIONEER);
    c6Config.can2Profile = decoderProfileNormalize(
        preferences.getUChar("can2", DECODER_PROFILE_DISPLAY_CAN),
        DECODER_PROFILE_DISPLAY_CAN);
    c6Config.wifiSsid = preferences.getString("ssid", "");
    c6Config.wifiPassword = preferences.getString("pass", "");
    c6Config.adminPassword = preferences.getString("otaPass", "");
    c6Config.setupPassword = preferences.getString("setupPass", "");
    c6Config.otaEnabled = preferences.getBool("otaEn", false);
    c6Config.publishIntervalMs = preferences.getUInt("pubMs", 5000);
    if (c6Config.publishIntervalMs < 1000) c6Config.publishIntervalMs = 1000;
    preferences.end();

    if (c6Config.setupPassword.length() < 12) {
        c6Config.setupPassword = LocalWebSecurity::generateRecoveryPassword();
        preferences.begin(PREFERENCES_NAMESPACE, false);
        preferences.putString("setupPass", c6Config.setupPassword);
        preferences.end();
    }

    Serial.printf("CAN configuration: CAN1=%s, CAN2=%s\n",
                  decoderProfileName(c6Config.can1Profile),
                  decoderProfileName(c6Config.can2Profile));
}

bool c6ConfigSetWifi(const String &ssidValue, const String &passwordValue)
{
    String ssid = ssidValue;
    String password = passwordValue;
    ssid.trim();
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63) return false;
    c6Config.wifiSsid = ssid;
    c6Config.wifiPassword = password;
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putString("ssid", c6Config.wifiSsid);
    preferences.putString("pass", c6Config.wifiPassword);
    preferences.end();
    return true;
}

void c6ConfigClearWifi()
{
    c6Config.wifiSsid = "";
    c6Config.wifiPassword = "";
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.remove("ssid");
    preferences.remove("pass");
    preferences.end();
}

void c6ConfigSave()
{
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putUChar("can1", static_cast<uint8_t>(c6Config.can1Profile));
    preferences.putUChar("can2", static_cast<uint8_t>(c6Config.can2Profile));
    preferences.putString("ssid", c6Config.wifiSsid);
    preferences.putString("pass", c6Config.wifiPassword);
    preferences.putString("otaPass", c6Config.adminPassword);
    preferences.putString("setupPass", c6Config.setupPassword);
    preferences.putBool("otaEn", c6Config.otaEnabled);
    preferences.putUInt("pubMs", c6Config.publishIntervalMs);
    preferences.end();
}

bool c6ConfigAdminConfigured()
{
    return validAdminPassword(c6Config.adminPassword);
}

bool c6ConfigSetAdminPassword(const String &value)
{
    String password = value;
    password.trim();
    if (!validAdminPassword(password)) return false;
    c6Config.adminPassword = password;
    return true;
}

String c6ConfigRecoverAdminPassword()
{
    c6Config.adminPassword = LocalWebSecurity::generateRecoveryPassword();
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putString("otaPass", c6Config.adminPassword);
    preferences.end();
    return c6Config.adminPassword;
}

String c6ConfigExportJson(bool includeSecrets)
{
    JsonDocument doc;
    doc["schemaVersion"] = 1;
    doc["board"] = MOT_BOARD;
    doc["wifiSsid"] = c6Config.wifiSsid;
    if (includeSecrets) doc["wifiPassword"] = c6Config.wifiPassword;
    doc["can1Profile"] = static_cast<int>(c6Config.can1Profile);
    doc["can2Profile"] = static_cast<int>(c6Config.can2Profile);
    doc["publishIntervalMs"] = c6Config.publishIntervalMs;
    doc["otaEnabled"] = c6Config.otaEnabled;
    if (includeSecrets) doc["adminPassword"] = c6Config.adminPassword;
    String result;
    serializeJsonPretty(doc, result);
    return result;
}

bool c6ConfigImportJson(const String &json, String &error)
{
    JsonDocument doc;
    const DeserializationError parseError = deserializeJson(doc, json);
    if (parseError) { error = parseError.c_str(); return false; }
    if (!doc["schemaVersion"].isNull() && doc["schemaVersion"].as<int>() > 1) {
        error = "unsupported schemaVersion";
        return false;
    }
    C6Configuration candidate = c6Config;
    if (!doc["wifiSsid"].isNull()) candidate.wifiSsid = doc["wifiSsid"].as<String>();
    if (!doc["wifiPassword"].isNull()) candidate.wifiPassword = doc["wifiPassword"].as<String>();
    if (!doc["can1Profile"].isNull()) candidate.can1Profile = decoderProfileNormalize(doc["can1Profile"].as<int>());
    if (!doc["can2Profile"].isNull()) candidate.can2Profile = decoderProfileNormalize(doc["can2Profile"].as<int>(), DECODER_PROFILE_DISABLED);
    if (!doc["publishIntervalMs"].isNull()) candidate.publishIntervalMs = doc["publishIntervalMs"].as<uint32_t>();
    if (!doc["otaEnabled"].isNull()) candidate.otaEnabled = doc["otaEnabled"].as<bool>();
    if (!doc["adminPassword"].isNull()) candidate.adminPassword = doc["adminPassword"].as<String>();
    candidate.wifiSsid.trim();
    if (candidate.wifiSsid.length() > 32 || candidate.wifiPassword.length() > 63 ||
        candidate.publishIntervalMs < 1000 || candidate.publishIntervalMs > 3600000) {
        error = "invalid WiFi or publish interval";
        return false;
    }
    const C6Configuration previous = c6Config;
    c6Config = candidate;
    if (!c6ConfigAdminConfigured()) {
        c6Config = previous;
        error = "local admin password must be 12-63 printable ASCII characters";
        return false;
    }
    c6ConfigSave();
    return true;
}

void c6ConfigFactoryReset()
{
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    c6Config = C6Configuration();
}

bool c6ConfigSetCanProfile(size_t channel, DecoderProfile profile)
{
    if (channel >= 2 || decoderProfileFind(profile) == nullptr) {
        return false;
    }

    if (!c6DualCanSetProfile(channel, profile)) {
        return false;
    }

    if (channel == 0) {
        c6Config.can1Profile = profile;
    } else {
        c6Config.can2Profile = profile;
    }
    c6ConfigSave();
    return true;
}
