#include "c6_config.h"

#include <Preferences.h>
#include <ArduinoJson.h>

#include "c6_dual_can.h"
#include "c6_offline_cache.h"
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
    c6Config.wifi2Ssid = preferences.getString("ssid2", "");
    c6Config.wifi2Password = preferences.getString("pass2", "");
    c6Config.adminPassword = preferences.getString("otaPass", "");
    c6Config.setupPassword = preferences.getString("setupPass", "");
    c6Config.motCloudEnabled = preferences.getBool("cloudEn", true);
    c6Config.abrpEnabled = preferences.getBool("abrpEn", false);
    c6Config.gpsEnabled = preferences.getBool("gpsEn", true);
    c6Config.offlineCacheEnabled = preferences.getBool("cacheEn", false);
    c6Config.abrpApiKey = preferences.getString("abrpKey", "");
    c6Config.abrpUserToken = preferences.getString("abrpToken", "");
    c6Config.onboardingComplete = preferences.isKey("onboarded")
        ? preferences.getBool("onboarded", false)
        : validAdminPassword(c6Config.adminPassword);
    c6Config.onboardingStep = preferences.getUChar(
        "onboardStep", c6Config.onboardingComplete ? 7 : 1);
    if (c6Config.onboardingStep < 1 || c6Config.onboardingStep > 7) {
        c6Config.onboardingStep = c6Config.onboardingComplete ? 7 : 1;
    }
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

bool c6ConfigSetWifi2(const String &ssidValue, const String &passwordValue)
{
    String ssid = ssidValue;
    String password = passwordValue;
    ssid.trim();
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63) return false;
    c6Config.wifi2Ssid = ssid;
    c6Config.wifi2Password = password;
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putString("ssid2", c6Config.wifi2Ssid);
    preferences.putString("pass2", c6Config.wifi2Password);
    preferences.end();
    return true;
}

void c6ConfigClearWifi2()
{
    c6Config.wifi2Ssid = "";
    c6Config.wifi2Password = "";
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.remove("ssid2");
    preferences.remove("pass2");
    preferences.end();
}

void c6ConfigSave()
{
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putUChar("can1", static_cast<uint8_t>(c6Config.can1Profile));
    preferences.putUChar("can2", static_cast<uint8_t>(c6Config.can2Profile));
    preferences.putString("ssid", c6Config.wifiSsid);
    preferences.putString("pass", c6Config.wifiPassword);
    preferences.putString("ssid2", c6Config.wifi2Ssid);
    preferences.putString("pass2", c6Config.wifi2Password);
    preferences.putString("otaPass", c6Config.adminPassword);
    preferences.putString("setupPass", c6Config.setupPassword);
    preferences.putBool("cloudEn", c6Config.motCloudEnabled);
    preferences.putBool("abrpEn", c6Config.abrpEnabled);
    preferences.putBool("gpsEn", c6Config.gpsEnabled);
    preferences.putBool("cacheEn", c6Config.offlineCacheEnabled);
    preferences.putString("abrpKey", c6Config.abrpApiKey);
    preferences.putString("abrpToken", c6Config.abrpUserToken);
    preferences.putBool("onboarded", c6Config.onboardingComplete);
    preferences.putUChar("onboardStep", c6Config.onboardingStep);
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

bool c6ConfigSetAbrpCredentials(const String &apiKeyValue, const String &userTokenValue)
{
    String apiKey = apiKeyValue;
    String userToken = userTokenValue;
    apiKey.trim(); userToken.trim();
    if (apiKey.length() > 192 || userToken.length() > 192) return false;
    c6Config.abrpApiKey = apiKey;
    c6Config.abrpUserToken = userToken;
    return true;
}

void c6ConfigClearAbrpCredentials()
{
    c6Config.abrpEnabled = false;
    c6Config.abrpApiKey = "";
    c6Config.abrpUserToken = "";
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool("abrpEn", false);
    preferences.remove("abrpKey");
    preferences.remove("abrpToken");
    preferences.end();
}

void c6ConfigSetAbrpEnabled(bool enabled)
{
    c6Config.abrpEnabled = enabled;
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool("abrpEn", enabled);
    preferences.end();
}

void c6ConfigSetMotCloudEnabled(bool enabled)
{
    c6Config.motCloudEnabled = enabled;
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool("cloudEn", enabled);
    preferences.end();
}

void c6ConfigSetOfflineCacheEnabled(bool enabled)
{
    const bool wasEnabled = c6Config.offlineCacheEnabled;
    c6Config.offlineCacheEnabled = enabled;
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool("cacheEn", enabled);
    preferences.end();
    if (wasEnabled && !enabled) c6OfflineCachePurge();
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
    doc["wifi2Ssid"] = c6Config.wifi2Ssid;
    if (includeSecrets) doc["wifi2Pass"] = c6Config.wifi2Password;
    doc["can1Profile"] = static_cast<int>(c6Config.can1Profile);
    doc["can2Profile"] = static_cast<int>(c6Config.can2Profile);
    doc["publishIntervalMs"] = c6Config.publishIntervalMs;
    doc["otaEnabled"] = c6Config.otaEnabled;
    doc["motCloudEnabled"] = c6Config.motCloudEnabled;
    doc["abrpEnabled"] = c6Config.abrpEnabled;
    doc["gpsEnabled"] = c6Config.gpsEnabled;
    doc["offlineCacheEnabled"] = c6Config.offlineCacheEnabled;
    doc["onboardingComplete"] = c6Config.onboardingComplete;
    doc["onboardingStep"] = c6Config.onboardingStep;
    if (includeSecrets) {
        doc["abrpApiKey"] = c6Config.abrpApiKey;
        doc["abrpUserToken"] = c6Config.abrpUserToken;
    }
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
    if (!doc["wifi2Ssid"].isNull()) candidate.wifi2Ssid = doc["wifi2Ssid"].as<String>();
    if (!doc["wifi2Pass"].isNull()) candidate.wifi2Password = doc["wifi2Pass"].as<String>();
    if (!doc["can1Profile"].isNull()) candidate.can1Profile = decoderProfileNormalize(doc["can1Profile"].as<int>());
    if (!doc["can2Profile"].isNull()) candidate.can2Profile = decoderProfileNormalize(doc["can2Profile"].as<int>(), DECODER_PROFILE_DISABLED);
    if (!doc["publishIntervalMs"].isNull()) candidate.publishIntervalMs = doc["publishIntervalMs"].as<uint32_t>();
    if (!doc["otaEnabled"].isNull()) candidate.otaEnabled = doc["otaEnabled"].as<bool>();
    if (!doc["motCloudEnabled"].isNull()) candidate.motCloudEnabled = doc["motCloudEnabled"].as<bool>();
    if (!doc["abrpEnabled"].isNull()) candidate.abrpEnabled = doc["abrpEnabled"].as<bool>();
    if (!doc["gpsEnabled"].isNull()) candidate.gpsEnabled = doc["gpsEnabled"].as<bool>();
    if (!doc["offlineCacheEnabled"].isNull()) candidate.offlineCacheEnabled = doc["offlineCacheEnabled"].as<bool>();
    if (!doc["abrpApiKey"].isNull()) candidate.abrpApiKey = doc["abrpApiKey"].as<String>();
    if (!doc["abrpUserToken"].isNull()) candidate.abrpUserToken = doc["abrpUserToken"].as<String>();
    if (!doc["onboardingComplete"].isNull()) candidate.onboardingComplete = doc["onboardingComplete"].as<bool>();
    if (!doc["onboardingStep"].isNull()) candidate.onboardingStep = doc["onboardingStep"].as<uint8_t>();
    if (!doc["adminPassword"].isNull()) candidate.adminPassword = doc["adminPassword"].as<String>();
    candidate.wifiSsid.trim();
    candidate.wifi2Ssid.trim();
    if (candidate.wifiSsid.length() > 32 || candidate.wifiPassword.length() > 63 ||
        candidate.wifi2Ssid.length() > 32 || candidate.wifi2Password.length() > 63 ||
        candidate.abrpApiKey.length() > 192 || candidate.abrpUserToken.length() > 192 ||
        candidate.publishIntervalMs < 1000 || candidate.publishIntervalMs > 3600000 ||
        candidate.onboardingStep < 1 || candidate.onboardingStep > 7) {
        error = "invalid WiFi, ABRP credentials, or publish interval";
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
    if (previous.offlineCacheEnabled && !c6Config.offlineCacheEnabled) c6OfflineCachePurge();
    return true;
}

void c6ConfigFactoryReset()
{
    c6OfflineCachePurge();
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
