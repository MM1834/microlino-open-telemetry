#include "app_config.h"
#include <Preferences.h>

AppConfig config;
static Preferences prefs;

void loadConfig()
{
    prefs.begin("mot", true);
    config.vehicleName = prefs.getString("vehicle", "microlino");
    config.mqttPrefix = prefs.getString("prefix", "mot/" + config.vehicleName);
    config.wifiSsid = prefs.getString("ssid", "");
    config.wifiPass = prefs.getString("pass", "");
    config.mqttHost = prefs.getString("mqttHost", "");
    config.mqttPort = prefs.getUShort("mqttPort", 1883);
    config.mqttUser = prefs.getString("mqttUser", "");
    config.mqttPass = prefs.getString("mqttPass", "");
    config.abrpApiKey = prefs.getString("abrpKey", "");
    config.abrpUserToken = prefs.getString("abrpToken", "");
    config.can1Profile = (DecoderProfile)prefs.getUChar("can1", DECODER_DISPLAY_CAN);
    config.can2Profile = (DecoderProfile)prefs.getUChar("can2", DECODER_BMS_1);
    config.otaEnabled = prefs.getBool("otaEn", false);
    config.otaPassword = prefs.getString("otaPass", "");
    config.publishIntervalMs = prefs.getUInt("pubMs", 5000);
    prefs.end();
}

void saveConfig()
{
    prefs.begin("mot", false);
    prefs.putString("vehicle", config.vehicleName);
    prefs.putString("prefix", config.mqttPrefix);
    prefs.putString("ssid", config.wifiSsid);
    prefs.putString("pass", config.wifiPass);
    prefs.putString("mqttHost", config.mqttHost);
    prefs.putUShort("mqttPort", config.mqttPort);
    prefs.putString("mqttUser", config.mqttUser);
    prefs.putString("mqttPass", config.mqttPass);
    prefs.putString("abrpKey", config.abrpApiKey);
    prefs.putString("abrpToken", config.abrpUserToken);
    prefs.putUChar("can1", config.can1Profile);
    prefs.putUChar("can2", config.can2Profile);
    prefs.putBool("otaEn", config.otaEnabled);
    prefs.putString("otaPass", config.otaPassword);
    prefs.putUInt("pubMs", config.publishIntervalMs);
    prefs.end();
}

void clearConfig()
{
    prefs.begin("mot", false);
    prefs.clear();
    prefs.end();
    config = AppConfig();
}

const char *decoderProfileName(DecoderProfile profile)
{
    switch (profile) {
        case DECODER_DISPLAY_CAN: return "Microlino Display CAN";
        case DECODER_BMS_1: return "Microlino CAN (BMS 1 / Pioneer)";
        case DECODER_BMS_2: return "Microlino CAN (BMS 2 / Standard CAN)";
        default: return "Unknown";
    }
}
