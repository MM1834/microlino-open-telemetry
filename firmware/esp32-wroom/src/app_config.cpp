#include "app_config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include "system/device_id.h"

AppConfig config;
static Preferences prefs;

void loadConfig()
{
    prefs.begin("mot", true);
    config.vehicleName = prefs.getString("vehicle", "Microlino Pioneer");
    config.deviceName = prefs.getString("deviceName", "");
    config.vehicleId = prefs.getString("vehicleId", "");
    config.mqttPrefix = prefs.getString("prefix", "mot");

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
    if (config.mqttPrefix.isEmpty()) config.mqttPrefix = "mot";
    config.deviceName.trim();
    config.deviceName.toLowerCase();
    config.deviceName.replace(" ", "-");
    config.deviceName.replace("/", "-");
    if (config.deviceName.isEmpty()) config.deviceName = motHostname();
    if (config.vehicleId.isEmpty()) config.vehicleId = "pioneer";
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
    prefs.putString("deviceName", config.deviceName);
    prefs.putString("vehicleId", config.vehicleId);
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


bool AppConfig::mqttEnabled() const
{
    String h = mqttHost;
    h.trim();
    return !h.isEmpty();
}

bool AppConfig::abrpEnabled() const
{
    String key = abrpApiKey;
    String token = abrpUserToken;
    key.trim();
    token.trim();
    return !key.isEmpty() && !token.isEmpty();
}

String AppConfig::mqttClientId() const
{
    String id = deviceName;
    id.trim();
    id.toLowerCase();
    id.replace(" ", "-");
    id.replace("/", "-");
    if (id.isEmpty()) id = motHostname();
    if (!id.startsWith("mot-")) id = "mot-" + id;
    return id;
}

String configToJson(bool includeSecrets)
{
    JsonDocument doc;

    doc["vehicleName"] = config.vehicleName;
    doc["vehicleId"] = config.vehicleId;
    doc["deviceName"] = config.deviceName;
    doc["mqttPrefix"] = config.mqttPrefix;

    doc["wifiSsid"] = config.wifiSsid;
    if (includeSecrets) doc["wifiPass"] = config.wifiPass;

    doc["mqttHost"] = config.mqttHost;
    doc["mqttPort"] = config.mqttPort;
    doc["mqttUser"] = config.mqttUser;
    if (includeSecrets) doc["mqttPass"] = config.mqttPass;
    doc["publishIntervalMs"] = config.publishIntervalMs;

    doc["abrpApiKey"] = config.abrpApiKey;
    if (includeSecrets) doc["abrpUserToken"] = config.abrpUserToken;

    doc["can1Profile"] = (int)config.can1Profile;
    doc["can2Profile"] = (int)config.can2Profile;

    doc["otaEnabled"] = config.otaEnabled;
    if (includeSecrets) doc["otaPassword"] = config.otaPassword;

    String out;
    serializeJsonPretty(doc, out);
    return out;
}

static void setStringIfPresent(JsonDocument& doc, const char *key, String& target)
{
    if (!doc[key].isNull()) target = doc[key].as<String>();
}

bool importConfigJson(const String& json, String& error)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        error = err.c_str();
        return false;
    }

    setStringIfPresent(doc, "vehicleName", config.vehicleName);
    setStringIfPresent(doc, "vehicleId", config.vehicleId);
    setStringIfPresent(doc, "deviceName", config.deviceName);
    setStringIfPresent(doc, "mqttPrefix", config.mqttPrefix);

    setStringIfPresent(doc, "wifiSsid", config.wifiSsid);
    setStringIfPresent(doc, "wifiPass", config.wifiPass);

    setStringIfPresent(doc, "mqttHost", config.mqttHost);
    if (!doc["mqttPort"].isNull()) config.mqttPort = doc["mqttPort"].as<uint16_t>();
    setStringIfPresent(doc, "mqttUser", config.mqttUser);
    setStringIfPresent(doc, "mqttPass", config.mqttPass);
    if (!doc["publishIntervalMs"].isNull()) config.publishIntervalMs = doc["publishIntervalMs"].as<uint32_t>();

    setStringIfPresent(doc, "abrpApiKey", config.abrpApiKey);
    setStringIfPresent(doc, "abrpUserToken", config.abrpUserToken);

    if (!doc["can1Profile"].isNull()) config.can1Profile = (DecoderProfile)doc["can1Profile"].as<int>();
    if (!doc["can2Profile"].isNull()) config.can2Profile = (DecoderProfile)doc["can2Profile"].as<int>();

    if (!doc["otaEnabled"].isNull()) config.otaEnabled = doc["otaEnabled"].as<bool>();
    setStringIfPresent(doc, "otaPassword", config.otaPassword);

    config.vehicleId.trim();
    config.vehicleId.toLowerCase();
    config.vehicleId.replace(" ", "-");
    config.vehicleId.replace("/", "-");
    if (config.vehicleId.isEmpty()) config.vehicleId = "pioneer";

    config.deviceName.trim();
    config.deviceName.toLowerCase();
    config.deviceName.replace(" ", "-");
    config.deviceName.replace("/", "-");
    if (config.deviceName.isEmpty()) config.deviceName = motHostname();

    config.mqttPrefix.trim();
    while (config.mqttPrefix.endsWith("/")) {
        config.mqttPrefix.remove(config.mqttPrefix.length() - 1);
    }
    if (config.mqttPrefix.isEmpty()) config.mqttPrefix = "mot";

    if (config.mqttPort == 0) config.mqttPort = 1883;
    if (config.publishIntervalMs < 1000) config.publishIntervalMs = 5000;

    saveConfig();
    return true;
}
