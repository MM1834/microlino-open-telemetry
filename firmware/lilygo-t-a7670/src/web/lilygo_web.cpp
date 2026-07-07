#include "lilygo_web.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "abrp/lilygo_abrp.h"
#include "api/telemetry_json.h"
#include "board_config.h"
#include "can/lilygo_can.h"
#include "config/lilygo_config.h"
#include "gps/l76k_gps.h"
#include "modem/lilygo_modem.h"
#include "mqtt/lilygo_mqtt.h"
#include "network/lilygo_network.h"
#include "telemetry/telemetry.h"

static WebServer server(80);
static bool rebootPending = false;
static unsigned long rebootAtMs = 0;

static String pageHeader(const char* title)
{
    String s;

    s += "<!doctype html><html><head><meta charset='utf-8'>";
    s += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<title>";
    s += title;
    s += "</title>";
    s += "<style>";
    s += "body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Arial,sans-serif;margin:24px;max-width:1000px}";
    s += ".card{border:1px solid #ddd;border-radius:12px;padding:16px;margin:12px 0}";
    s += "pre,textarea{background:#111;color:#eee;border-radius:8px;padding:12px;overflow:auto;white-space:pre-wrap}";
    s += "textarea{width:100%;box-sizing:border-box;min-height:180px;font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace}";
    s += ".muted{color:#666}";
    s += "button{padding:10px 16px;border-radius:8px;border:1px solid #999;background:#f5f5f5;margin:4px}";
    s += "input{padding:9px;width:100%;box-sizing:border-box;margin:6px 0 10px}";
    s += "label{font-weight:600}";
    s += "</style></head><body>";

    return s;
}

static void handleRoot()
{
    String s = pageHeader("MOT LilyGO");

    s += "<h1>Microlino Open Telemetry</h1>";
    s += "<p class='muted'>";
    s += MOT_VERSION;
    s += " · ";
    s += MOT_BOARD;
    s += " · ";
    s += lilygoDeviceName();
    s += "</p>";

    s += "<p><a href='/config'>Config</a> · <a href='/ota'>OTA</a> · <a href='/api/status'>Status JSON</a></p>";

    s += "<div class='card'><h2>Network</h2><button onclick='loadNetwork()'>Refresh</button><pre id='network'>Loading...</pre></div>";
    s += "<div class='card'><h2>LTE Modem</h2><p><a href='/api/lilygo/lte/debug'>LTE Debug</a> · <a href='/api/lilygo/lte/rx-debug'>LTE RX Debug</a> · <a href='/api/lilygo/lte/tcp-test'>LTE TCP Test</a></p><button onclick='loadModem()'>Refresh</button><pre id='modem'>Loading...</pre></div>";
    s += "<div class='card'><h2>L76K GPS</h2><button onclick='loadGps()'>Refresh</button><pre id='gps'>Loading...</pre></div>";
    s += "<div class='card'><h2>CAN Input</h2><button onclick='loadCan()'>Refresh</button><pre id='can'>Loading...</pre><p><a href='/api/lilygo/can/frames'>Latest frames JSON</a></p></div>";
    s += "<div class='card'><h2>Decoded Telemetry</h2><button onclick='loadTelemetry()'>Refresh</button><pre id='telemetry'>Loading...</pre></div>";

    s += "<div class='card'><h2>MQTT WiFi</h2>";
    s += "<button onclick='loadMqtt()'>Refresh</button>";
    s += "<pre id='mqtt'>Loading...</pre></div>";

    s += "<div class='card'><h2>ABRP</h2>";
    s += "<button onclick='loadAbrp()'>Refresh</button>";
    s += "<button onclick='testAbrp()'>Test send</button>";
    s += "<pre id='abrp'>Loading...</pre></div>";

    s += "<script>";
    s += "async function loadNetwork(){const r=await fetch('/api/lilygo/network');document.getElementById('network').textContent=JSON.stringify(await r.json(),null,2)}";
    s += "async function loadModem(){const r=await fetch('/api/lilygo/modem');document.getElementById('modem').textContent=JSON.stringify(await r.json(),null,2)}";
    s += "async function loadGps(){const r=await fetch('/api/lilygo/gps');document.getElementById('gps').textContent=JSON.stringify(await r.json(),null,2)}";
    s += "async function loadCan(){const r=await fetch('/api/lilygo/can');document.getElementById('can').textContent=JSON.stringify(await r.json(),null,2)}";
    s += "async function loadTelemetry(){const r=await fetch('/api/telemetry');document.getElementById('telemetry').textContent=JSON.stringify(await r.json(),null,2)}";
    s += "async function loadMqtt(){const r=await fetch('/api/lilygo/mqtt');document.getElementById('mqtt').textContent=JSON.stringify(await r.json(),null,2)}";
    s += "async function loadAbrp(){const r=await fetch('/api/lilygo/abrp');document.getElementById('abrp').textContent=JSON.stringify(await r.json(),null,2)}";
    s += "async function testAbrp(){const r=await fetch('/api/lilygo/abrp/test',{method:'POST'});document.getElementById('abrp').textContent=JSON.stringify(await r.json(),null,2)}";
    s += "loadNetwork();loadModem();loadGps();loadCan();loadTelemetry();loadMqtt();loadAbrp();";
    s += "setInterval(loadNetwork,5000);setInterval(loadGps,3000);setInterval(loadCan,3000);setInterval(loadTelemetry,3000);setInterval(loadMqtt,5000);setInterval(loadAbrp,10000);";
    s += "</script></body></html>";

    server.send(200, "text/html", s);
}

static void handleConfig()
{
    String s = pageHeader("MOT LilyGO Config");

    s += "<h1>Configuration</h1>";

    s += "<form method='POST' action='/config/save'>";
    s += "<label>Device name</label><input name='deviceName' value='" + lilygoDeviceName() + "'>";
    s += "<label>Vehicle ID</label><input name='vehicleId' value='" + config.vehicleId + "'>";
    s += "<label>MQTT Prefix</label><input name='mqttPrefix' value='" + config.mqttPrefix + "'>";

    s += "<label>WiFi SSID</label><input name='wifiSsid' value='" + config.wifiSsid + "'>";
    s += "<label>WiFi Password</label><input name='wifiPass' type='password' value='" + config.wifiPass + "'>";

    s += "<label>LTE APN</label><input name='lteApn' value='" + config.lteApn + "'>";

    s += "<label>MQTT Host</label><input name='mqttHost' value='" + config.mqttHost + "'>";
    s += "<label>MQTT Port</label><input name='mqttPort' value='" + String(config.mqttPort) + "'>";
    s += "<label>MQTT User</label><input name='mqttUser' value='" + config.mqttUser + "'>";
    s += "<label>MQTT Password</label><input name='mqttPass' type='password' value='" + config.mqttPass + "'>";

    s += "<label>OTA Password</label><input name='otaPassword' type='password' value='" + config.otaPassword + "'>";

    s += "<label>ABRP enabled</label><input name='abrpEnabled' value='" + String(config.abrpEnabled ? "1" : "0") + "'>";
    s += "<label>ABRP API Key</label><input name='abrpApiKey' type='password' value='" + config.abrpApiKey + "'>";
    s += "<label>ABRP User Token</label><input name='abrpUserToken' type='password' value='" + config.abrpUserToken + "'>";

    s += "<button type='submit'>Save & reboot</button></form>";

    s += "<div class='card'><h2>Backup</h2>";
    s += "<p><a href='/api/config/export'>Download config JSON</a></p>";
    s += "</div>";

    s += "<div class='card'><h2>Restore</h2>";
    s += "<form method='POST' action='/config/import' onsubmit=\"return confirm('Configuration restore ausführen? Aktuelle Einstellungen werden überschrieben.');\">";
    s += "<label>Config JSON file</label><input type='file' id='restoreFile' accept='application/json,.json'>";
    s += "<label>Config JSON</label><textarea id='restoreJson' name='configJson' placeholder='Backup JSON hier einfügen oder Datei auswählen'></textarea>";
    s += "<button type='submit'>Restore config & reboot</button></form>";
    s += "<script>";
    s += "const rf=document.getElementById('restoreFile');if(rf){rf.addEventListener('change',async e=>{const f=e.target.files[0];if(f){document.getElementById('restoreJson').value=await f.text();}})}";
    s += "</script>";
    s += "</div>";

    s += "<div class='card'><h2>Factory Reset</h2>";
    s += "<form method='POST' action='/factory-reset' onsubmit=\"return confirm('Factory Reset wirklich ausführen? Alle Einstellungen werden gelöscht.');\">";
    s += "<button type='submit'>Clear config & reboot</button></form></div>";

    s += "<p><a href='/'>Back</a></p></body></html>";

    server.send(200, "text/html", s);
}

static void handleConfigSave()
{
    config.deviceName = server.arg("deviceName");
    config.vehicleId = server.arg("vehicleId");
    config.mqttPrefix = server.arg("mqttPrefix");

    config.wifiSsid = server.arg("wifiSsid");
    config.wifiPass = server.arg("wifiPass");

    config.lteApn = server.arg("lteApn");

    config.mqttHost = server.arg("mqttHost");
    config.mqttPort = (uint16_t)server.arg("mqttPort").toInt();
    if (config.mqttPort == 0) config.mqttPort = 1883;
    config.mqttUser = server.arg("mqttUser");
    config.mqttPass = server.arg("mqttPass");

    config.otaPassword = server.arg("otaPassword");

    config.abrpEnabled =
        server.arg("abrpEnabled") == "1" ||
        server.arg("abrpEnabled") == "true" ||
        server.arg("abrpEnabled") == "on";
    config.abrpApiKey = server.arg("abrpApiKey");
    config.abrpUserToken = server.arg("abrpUserToken");

    saveLilygoConfig();

    server.send(200, "text/html", "<p>Saved. Rebooting...</p>");
    rebootPending = true;
    rebootAtMs = millis() + 1000;
}

static void handleConfigImport()
{
    if (!server.hasArg("configJson")) {
        server.send(400, "text/plain", "Missing configJson field");
        return;
    }

    String body = server.arg("configJson");
    body.trim();

    if (body.isEmpty()) {
        server.send(400, "text/plain", "Empty config JSON");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);

    if (err) {
        server.send(400, "text/plain", String("Invalid JSON: ") + err.c_str());
        return;
    }

    if (doc["deviceName"].is<const char*>()) config.deviceName = doc["deviceName"].as<String>();
    if (doc["vehicleId"].is<const char*>()) config.vehicleId = doc["vehicleId"].as<String>();
    if (doc["mqttPrefix"].is<const char*>()) config.mqttPrefix = doc["mqttPrefix"].as<String>();

    if (doc["wifiSsid"].is<const char*>()) config.wifiSsid = doc["wifiSsid"].as<String>();
    if (doc["wifiPass"].is<const char*>()) config.wifiPass = doc["wifiPass"].as<String>();

    if (doc["lteApn"].is<const char*>()) config.lteApn = doc["lteApn"].as<String>();
    if (doc["lteUser"].is<const char*>()) config.lteUser = doc["lteUser"].as<String>();
    if (doc["ltePass"].is<const char*>()) config.ltePass = doc["ltePass"].as<String>();

    if (doc["mqttHost"].is<const char*>()) config.mqttHost = doc["mqttHost"].as<String>();
    if (doc["mqttPort"].is<int>()) config.mqttPort = (uint16_t)doc["mqttPort"].as<int>();
    if (config.mqttPort == 0) config.mqttPort = 1883;
    if (doc["mqttUser"].is<const char*>()) config.mqttUser = doc["mqttUser"].as<String>();
    if (doc["mqttPass"].is<const char*>()) config.mqttPass = doc["mqttPass"].as<String>();

    if (doc["otaEnabled"].is<bool>()) config.otaEnabled = doc["otaEnabled"].as<bool>();
    if (doc["otaPassword"].is<const char*>()) config.otaPassword = doc["otaPassword"].as<String>();

    if (doc["abrpEnabled"].is<bool>()) config.abrpEnabled = doc["abrpEnabled"].as<bool>();
    if (doc["abrpEnabled"].is<int>()) config.abrpEnabled = doc["abrpEnabled"].as<int>() != 0;
    if (doc["abrpApiKey"].is<const char*>()) config.abrpApiKey = doc["abrpApiKey"].as<String>();
    if (doc["abrpUserToken"].is<const char*>()) config.abrpUserToken = doc["abrpUserToken"].as<String>();

    saveLilygoConfig();

    server.send(200, "text/html", "<p>Config restored. Rebooting...</p>");
    rebootPending = true;
    rebootAtMs = millis() + 1000;
}

static void handleFactoryReset()
{
    clearLilygoConfig();

    server.send(200, "text/html", "<p>Config cleared. Rebooting...</p>");
    rebootPending = true;
    rebootAtMs = millis() + 1000;
}

static void handleConfigExport()
{
    server.sendHeader("Content-Disposition", "attachment; filename=mot-lilygo-config.json");
    server.send(200, "application/json", lilygoConfigJson(true));
}

static void handleStatusJson()
{
    String json = "{";
    json += "\"firmware\":\"" MOT_VERSION "\",";
    json += "\"board\":\"" MOT_BOARD "\",";
    json += "\"deviceName\":\"" + lilygoDeviceName() + "\",";
    json += "\"network\":" + lilygoNetworkStatusJson() + ",";
    json += "\"modem\":" + lilygoModemStatusJson() + ",";
    json += "\"gps\":" + l76kGpsStatusJson() + ",";
    json += "\"can\":" + lilygoCanStatusJson() + ",";
    json += "\"mqtt\":" + lilygoMqttStatusJson() + ",";
    json += "\"abrp\":" + lilygoAbrpStatusJson() + ",";
    json += "\"telemetry\":" + telemetryToJson(telemetry);
    json += "}";

    server.send(200, "application/json", json);
}

static void handleAbrp()
{
    server.send(200, "application/json", lilygoAbrpStatusJson());
}

static void handleAbrpTest()
{
    server.send(sendLilygoAbrpTelemetryNow() ? 200 : 503, "application/json", lilygoAbrpStatusJson());
}



static void handleLteTcpTest()
{
    String host = server.hasArg("host") ? server.arg("host") : config.mqttHost;
    uint16_t port = server.hasArg("port") ? (uint16_t)server.arg("port").toInt() : config.mqttPort;

    host.trim();

    if (host.isEmpty() || port == 0) {
        server.send(400, "application/json", "{\"error\":\"missing host or port\"}");
        return;
    }

    server.send(200, "application/json", lilygoLteTcpTestJson(host, port));
}


static void handleLteRxDebug()
{
    server.send(200, "application/json", lilygoLteRxDebugJson());
}

static void handleLteDebug()
{
    server.send(200, "application/json", lilygoLteDebugJson());
}


static void handleMqtt()
{
    server.send(200, "application/json", lilygoMqttStatusJson());
}

static void handleMqttDebug()
{
    server.send(200, "application/json", lilygoMqttDebugJson());
}

static void handleTelemetry()
{
    server.send(200, "application/json", telemetryToJson(telemetry));
}

static void handleModem()
{
    server.send(200, "application/json", lilygoModemStatusJson());
}

static void handleNetwork()
{
    server.send(200, "application/json", lilygoNetworkStatusJson());
}

static void handleCan()
{
    server.send(200, "application/json", lilygoCanStatusJson());
}

static void handleCanFrames()
{
    server.send(200, "application/json", lilygoCanFramesJson());
}

static void handleGps()
{
    server.send(200, "application/json", l76kGpsStatusJson());
}

static bool otaAllowed()
{
    String pass = config.otaPassword;
    pass.trim();

    if (pass.isEmpty()) return true;

    return server.hasArg("password") && server.arg("password") == pass;
}

static void handleOtaPage()
{
    String s = pageHeader("MOT LilyGO OTA");

    s += "<h1>OTA Update</h1>";
    s += "<form method='POST' action='/ota/update' enctype='multipart/form-data'>";
    s += "<label>Password</label><input name='password' type='password'>";
    s += "<input type='file' name='firmware'>";
    s += "<button type='submit'>Upload firmware</button></form>";
    s += "<p><a href='/'>Back</a></p></body></html>";

    server.send(200, "text/html", s);
}

static void handleOtaDone()
{
    if (!otaAllowed()) {
        server.send(403, "text/plain", "OTA not allowed");
        return;
    }

    bool ok = !Update.hasError();

    server.send(ok ? 200 : 500, "text/plain", ok ? "OTA OK. Rebooting." : "OTA failed.");

    if (ok) {
        rebootPending = true;
        rebootAtMs = millis() + 1000;
    }
}

static void handleOtaUpload()
{
    if (!otaAllowed()) return;

    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        Update.end(true);
    }
}

void setupLilygoWeb()
{
    server.on("/", HTTP_GET, handleRoot);
    server.on("/config", HTTP_GET, handleConfig);
    server.on("/config/save", HTTP_POST, handleConfigSave);
    server.on("/config/import", HTTP_POST, handleConfigImport);
    server.on("/factory-reset", HTTP_POST, handleFactoryReset);

    server.on("/api/config/export", HTTP_GET, handleConfigExport);
    server.on("/api/status", HTTP_GET, handleStatusJson);

    server.on("/api/lilygo/network", HTTP_GET, handleNetwork);
    server.on("/api/lilygo/lte/debug", HTTP_GET, handleLteDebug);
    server.on("/api/lilygo/lte/rx-debug", HTTP_GET, handleLteRxDebug);
    server.on("/api/lilygo/lte/tcp-test", HTTP_GET, handleLteTcpTest);
    server.on("/api/lilygo/lte/tcp-test", HTTP_POST, handleLteTcpTest);
    server.on("/api/lilygo/abrp", HTTP_GET, handleAbrp);
    server.on("/api/lilygo/abrp/test", HTTP_POST, handleAbrpTest);
    server.on("/api/lilygo/mqtt", HTTP_GET, handleMqtt);
    server.on("/api/lilygo/mqtt/debug", HTTP_GET, handleMqttDebug);
    server.on("/api/telemetry", HTTP_GET, handleTelemetry);
    server.on("/api/lilygo/modem", HTTP_GET, handleModem);
    server.on("/api/lilygo/can", HTTP_GET, handleCan);
    server.on("/api/lilygo/can/frames", HTTP_GET, handleCanFrames);
    server.on("/api/lilygo/gps", HTTP_GET, handleGps);
    server.on("/api/lilygo/gnss", HTTP_GET, handleGps);

    server.on("/ota", HTTP_GET, handleOtaPage);
    server.on("/ota/update", HTTP_POST, handleOtaDone, handleOtaUpload);

    server.begin();
    Serial.println("LilyGO Web UI started");
}

void lilygoWebLoop()
{
    server.handleClient();

    if (rebootPending && millis() > rebootAtMs) {
        ESP.restart();
    }
}
