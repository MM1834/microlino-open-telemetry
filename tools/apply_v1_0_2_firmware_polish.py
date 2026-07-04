
from pathlib import Path

root = Path.cwd()

def read(path):
    return path.read_text(encoding="utf-8")

def write(path, text):
    path.write_text(text, encoding="utf-8")

def replace_once(text, old, new, label):
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"Patch marker not found: {label}")
    return text.replace(old, new, 1)

# app_config.h
h = root / "firmware/esp32-wroom/src/app_config.h"
txt = read(h)

txt = replace_once(
    txt,
    '    String vehicleName = "Microlino Pioneer";   // Display name only\n',
    '    String vehicleName = "Microlino Pioneer";   // Display name only\n'
    '    String deviceName;                          // Stable hostname / MQTT client id\n',
    "deviceName in app_config.h"
)

txt = replace_once(
    txt,
    '    uint32_t publishIntervalMs = 5000;\n',
    '    uint32_t publishIntervalMs = 5000;\n\n'
    '    bool mqttEnabled() const;\n'
    '    bool abrpEnabled() const;\n'
    '    String mqttClientId() const;\n',
    "helpers in app_config.h"
)

txt = replace_once(
    txt,
    'void clearConfig();\n',
    'void clearConfig();\n'
    'String configToJson(bool includeSecrets = true);\n'
    'bool importConfigJson(const String& json, String& error);\n',
    "json declarations in app_config.h"
)

write(h, txt)

# app_config.cpp
cpp = root / "firmware/esp32-wroom/src/app_config.cpp"
txt = read(cpp)

if '#include <ArduinoJson.h>' not in txt:
    txt = txt.replace('#include <Preferences.h>\n', '#include <Preferences.h>\n#include <ArduinoJson.h>\n')
if '#include "system/device_id.h"' not in txt:
    txt = txt.replace('#include <ArduinoJson.h>\n', '#include <ArduinoJson.h>\n#include "system/device_id.h"\n')

txt = replace_once(
    txt,
    '    config.vehicleName = prefs.getString("vehicle", "Microlino Pioneer");\n',
    '    config.vehicleName = prefs.getString("vehicle", "Microlino Pioneer");\n'
    '    config.deviceName = prefs.getString("deviceName", "");\n',
    "load deviceName"
)

txt = replace_once(
    txt,
    '    if (config.mqttPrefix.isEmpty()) config.mqttPrefix = "mot";\n',
    '    if (config.mqttPrefix.isEmpty()) config.mqttPrefix = "mot";\n'
    '    config.deviceName.trim();\n'
    '    config.deviceName.toLowerCase();\n'
    '    config.deviceName.replace(" ", "-");\n'
    '    config.deviceName.replace("/", "-");\n'
    '    if (config.deviceName.isEmpty()) config.deviceName = motHostname();\n',
    "default deviceName"
)

txt = replace_once(
    txt,
    '    prefs.putString("vehicle", config.vehicleName);\n',
    '    prefs.putString("vehicle", config.vehicleName);\n'
    '    prefs.putString("deviceName", config.deviceName);\n',
    "save deviceName"
)

if 'bool AppConfig::mqttEnabled() const' not in txt:
    txt += r'''

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
'''
write(cpp, txt)

# mqtt_client.cpp
mqtt = root / "firmware/esp32-wroom/src/mqtt/mqtt_client.cpp"
txt = read(mqtt)
txt = txt.replace('    if (!networkOnline() || config.mqttHost.isEmpty() || mqtt.connected()) return;\n',
                  '    if (!networkOnline() || !config.mqttEnabled() || mqtt.connected()) return;\n')
txt = txt.replace('    String clientId = motDeviceId();\n',
                  '    String clientId = config.mqttClientId();\n')
txt = txt.replace(
    '    if (!config.mqttHost.isEmpty()) {\n        mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);\n    }\n',
    '    if (config.mqttEnabled()) {\n'
    '        mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);\n'
    '        Serial.printf("MQTT: enabled host=%s port=%u clientId=%s\\n", config.mqttHost.c_str(), config.mqttPort, config.mqttClientId().c_str());\n'
    '    } else {\n'
    '        Serial.println("MQTT: disabled (no host configured)");\n'
    '    }\n'
)
txt = txt.replace('    if (!mqtt.connected()) reconnectMqtt();\n    mqtt.loop();\n',
                  '    if (!config.mqttEnabled()) return;\n    if (!mqtt.connected()) reconnectMqtt();\n    mqtt.loop();\n')
txt = txt.replace('    if (!mqtt.connected()) return;\n',
                  '    if (!config.mqttEnabled() || !mqtt.connected()) return;\n')
txt = txt.replace('    mqtt.publish(topic("system/device_id").c_str(), motDeviceId().c_str(), true);\n',
                  '    mqtt.publish(topic("system/device_id").c_str(), motDeviceId().c_str(), true);\n'
                  '    mqtt.publish(topic("system/device_name").c_str(), config.deviceName.c_str(), true);\n'
                  '    mqtt.publish(topic("system/mqtt_client_id").c_str(), config.mqttClientId().c_str(), true);\n')
write(mqtt, txt)

# main.cpp
main = root / "firmware/esp32-wroom/src/main.cpp"
txt = read(main)
if '#include "common/abrp/abrp_client.h"' not in txt:
    txt = txt.replace('#include "system/device_id.h"\n', '#include "system/device_id.h"\n#include "common/abrp/abrp_client.h"\n')
if 'setupAbrp();' not in txt:
    txt = txt.replace('    setupMqtt();\n', '    setupMqtt();\n    setupAbrp();\n')
if '    abrpLoop();\n' not in txt:
    txt = txt.replace('    webUiLoop();\n', '    webUiLoop();\n    abrpLoop();\n')
write(main, txt)

# web_ui.cpp
web = root / "firmware/esp32-wroom/src/web/web_ui.cpp"
txt = read(web)
txt = txt.replace('    s += "Vehicle name<input name=\'vehicleName\' value=\'" + config.vehicleName + "\'>";\n',
                  '    s += "Vehicle name<input name=\'vehicleName\' value=\'" + config.vehicleName + "\'>";\n'
                  '    s += "Device name<input name=\'deviceName\' value=\'" + config.deviceName + "\'>";\n'
                  '    s += "<p class=\'muted\'>Stable device hostname / MQTT client ID. If empty, a stable MAC-based ID is used.</p>";\n')
txt = txt.replace('    s += "<div class=\'card\'><h2>MQTT</h2>";\n',
                  '    s += "<div class=\'card\'><h2>MQTT</h2>";\n'
                  '    s += "<p class=\'muted\'>MQTT is optional. Leave host empty to disable MQTT without connection errors.</p>";\n')
txt = txt.replace('    s += "<div class=\'card\'><h2>OTA / ABRP</h2>";\n',
                  '    s += "<div class=\'card\'><h2>OTA / ABRP</h2>";\n'
                  '    s += "<p class=\'muted\'>ABRP is optional. It is only active when API key and user token are both configured.</p>";\n')
if "/api/config/export" not in txt:
    marker = '    s += "<div class=\'card\'><h2>Factory Reset</h2><form method=\'POST\' action=\'/factory-reset\'><button type=\'submit\'>Clear config & reboot</button></form></div>";\n'
    repl = (
        '    s += "<div class=\'card\'><h2>Configuration Management</h2>";\n'
        '    s += "<p><a href=\'/api/config/export\'>Download config JSON</a></p>";\n'
        '    s += "<form method=\'POST\' action=\'/config/import\'>";\n'
        '    s += "<textarea name=\'configJson\' rows=\'8\' style=\'width:100%;box-sizing:border-box\' placeholder=\'Paste config JSON here\'></textarea>";\n'
        '    s += "<button type=\'submit\'>Import config & reboot</button></form>";\n'
        '    s += "<p class=\'muted\'>Export contains local secrets such as WiFi and MQTT passwords. Keep it private.</p></div>";\n'
        + marker
    )
    txt = txt.replace(marker, repl)
txt = txt.replace('    config.vehicleName = server.arg("vehicleName");\n    if (config.vehicleName.isEmpty()) config.vehicleName = "Microlino Pioneer";\n',
                  '    config.vehicleName = server.arg("vehicleName");\n'
                  '    if (config.vehicleName.isEmpty()) config.vehicleName = "Microlino Pioneer";\n\n'
                  '    config.deviceName = server.arg("deviceName");\n'
                  '    config.deviceName.trim();\n'
                  '    config.deviceName.toLowerCase();\n'
                  '    config.deviceName.replace(" ", "-");\n'
                  '    config.deviceName.replace("/", "-");\n'
                  '    if (config.deviceName.isEmpty()) config.deviceName = motHostname();\n')
handlers = r'''
static void handleConfigExport()
{
    server.sendHeader("Content-Disposition", "attachment; filename=mot-config.json");
    server.send(200, "application/json", configToJson(true));
}

static void handleConfigImport()
{
    String json = server.arg("configJson");
    if (json.isEmpty()) json = server.arg("plain");

    String error;
    if (!importConfigJson(json, error)) {
        server.send(400, "text/plain", "Config import failed: " + error);
        return;
    }

    server.send(200, "text/html", "<!doctype html><html><body style='font-family:sans-serif;text-align:center;margin-top:60px'><h2>Configuration imported.</h2><p>Device will reboot in 5 seconds...</p></body></html>");
    rebootPending = true;
    rebootAtMs = millis() + 5000;
}

'''
if 'static void handleConfigExport()' not in txt:
    txt = txt.replace('static void handleFactoryReset()\n', handlers + 'static void handleFactoryReset()\n')
if 'server.on("/api/config/export"' not in txt:
    txt = txt.replace('    server.on("/factory-reset", HTTP_POST, handleFactoryReset);\n',
                      '    server.on("/api/config/export", HTTP_GET, handleConfigExport);\n'
                      '    server.on("/config/import", HTTP_POST, handleConfigImport);\n'
                      '    server.on("/factory-reset", HTTP_POST, handleFactoryReset);\n')
write(web, txt)

print("MOT v1.0.2 firmware polish patch applied.")
