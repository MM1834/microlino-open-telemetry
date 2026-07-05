#include "lilygo_config.h"
#include <Preferences.h>

LilygoConfig config;
static Preferences prefs;

static String chipSuffix()
{
    uint64_t mac = ESP.getEfuseMac();
    char buf[16];
    snprintf(buf, sizeof(buf), "%06X", (uint32_t)(mac & 0xFFFFFF));
    return String(buf);
}

String lilygoDeviceName()
{
    String n = config.deviceName;
    n.trim();
    n.toLowerCase();
    n.replace(" ", "-");
    n.replace("/", "-");
    if (n.isEmpty()) n = "mot-lilygo-" + chipSuffix();
    return n;
}

static String getStringOrDefault(const char* key, const char* fallback)
{
    if (!prefs.isKey(key)) return String(fallback);
    return prefs.getString(key, fallback);
}

void loadLilygoConfig()
{
    prefs.begin("mot-lg", false);

    config.wifiSsid = getStringOrDefault("wifiSsid", "");
    config.wifiPass = getStringOrDefault("wifiPass", "");
    config.lteApn = getStringOrDefault("lteApn", "gprs.swisscom.ch");
    config.lteUser = getStringOrDefault("lteUser", "");
    config.ltePass = getStringOrDefault("ltePass", "");
    config.mqttHost = getStringOrDefault("mqttHost", "");
    config.mqttPort = prefs.isKey("mqttPort") ? prefs.getUShort("mqttPort", 1883) : 1883;
    config.mqttUser = getStringOrDefault("mqttUser", "");
    config.mqttPass = getStringOrDefault("mqttPass", "");
    config.deviceName = getStringOrDefault("deviceName", "");
    config.vehicleId = getStringOrDefault("vehicleId", "pioneer");
    config.mqttPrefix = getStringOrDefault("mqttPrefix", "mot");
    config.otaEnabled = prefs.isKey("otaEnabled") ? prefs.getBool("otaEnabled", true) : true;
    config.otaPassword = getStringOrDefault("otaPassword", "");

    prefs.end();

    config.lteApn.trim();
    if (config.lteApn.isEmpty()) config.lteApn = "gprs.swisscom.ch";
    if (config.vehicleId.isEmpty()) config.vehicleId = "pioneer";
    if (config.mqttPrefix.isEmpty()) config.mqttPrefix = "mot";
}

void saveLilygoConfig()
{
    prefs.begin("mot-lg", false);
    prefs.putString("wifiSsid", config.wifiSsid);
    prefs.putString("wifiPass", config.wifiPass);
    prefs.putString("lteApn", config.lteApn);
    prefs.putString("lteUser", config.lteUser);
    prefs.putString("ltePass", config.ltePass);
    prefs.putString("mqttHost", config.mqttHost);
    prefs.putUShort("mqttPort", config.mqttPort);
    prefs.putString("mqttUser", config.mqttUser);
    prefs.putString("mqttPass", config.mqttPass);
    prefs.putString("deviceName", config.deviceName);
    prefs.putString("vehicleId", config.vehicleId);
    prefs.putString("mqttPrefix", config.mqttPrefix);
    prefs.putBool("otaEnabled", config.otaEnabled);
    prefs.putString("otaPassword", config.otaPassword);
    prefs.end();
}

void clearLilygoConfig()
{
    prefs.begin("mot-lg", false);
    prefs.clear();
    prefs.end();
    config = LilygoConfig();
}

static String esc(String s)
{
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    s.replace("\r", "\\r");
    s.replace("\n", "\\n");
    return s;
}

String lilygoConfigJson(bool includeSecrets)
{
    String json = "{";
    json += "\"deviceName\":\"" + esc(lilygoDeviceName()) + "\",";
    json += "\"vehicleId\":\"" + esc(config.vehicleId) + "\",";
    json += "\"mqttPrefix\":\"" + esc(config.mqttPrefix) + "\",";
    json += "\"wifiSsid\":\"" + esc(config.wifiSsid) + "\",";
    json += "\"lteApn\":\"" + esc(config.lteApn) + "\",";
    json += "\"mqttHost\":\"" + esc(config.mqttHost) + "\",";
    json += "\"mqttPort\":" + String(config.mqttPort) + ",";
    json += "\"otaEnabled\":" + String(config.otaEnabled ? "true" : "false");
    if (includeSecrets) {
        json += ",\"wifiPass\":\"" + esc(config.wifiPass) + "\"";
        json += ",\"lteUser\":\"" + esc(config.lteUser) + "\"";
        json += ",\"ltePass\":\"" + esc(config.ltePass) + "\"";
        json += ",\"mqttUser\":\"" + esc(config.mqttUser) + "\"";
        json += ",\"mqttPass\":\"" + esc(config.mqttPass) + "\"";
        json += ",\"otaPassword\":\"" + esc(config.otaPassword) + "\"";
    }
    json += "}";
    return json;
}
