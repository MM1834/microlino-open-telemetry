#include "c6_config.h"

#include <Preferences.h>

#include "c6_dual_can.h"

namespace {
constexpr char PREFERENCES_NAMESPACE[] = "mot";
Preferences preferences;
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
    c6Config.publishIntervalMs = preferences.getUInt("pubMs", 5000);
    if (c6Config.publishIntervalMs < 1000) c6Config.publishIntervalMs = 1000;
    preferences.end();

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
    preferences.end();
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
