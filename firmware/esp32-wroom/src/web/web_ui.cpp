#include "web_ui.h"
#include "../app_config.h"
#include "../network/wifi_manager.h"
#include "../ota/ota_web.h"

#include <Arduino.h>
#include <WebServer.h>
#include "telemetry/telemetry.h"
#include "api/telemetry_json.h"
#include "system/device_id.h"
#include "system/version.h"
#include "MqttDiagnostics.h"
#include "SystemHealth.h"
#include <WiFi.h>

static WebServer server(80);
static bool rebootPending = false;
static unsigned long rebootAtMs = 0;

static String htmlHeader(const char *title)
{
    String s;
    s += "<!doctype html><html><head><meta charset='utf-8'>";
    s += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<title>"; s += title; s += "</title>";
    s += "<style>body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Arial,sans-serif;margin:24px;max-width:760px}";
    s += "input,select{width:100%;padding:9px;margin:4px 0 12px;box-sizing:border-box}";
    s += "button{padding:10px 16px;border-radius:8px;border:1px solid #999;background:#f5f5f5}";
    s += ".card{border:1px solid #ddd;border-radius:12px;padding:16px;margin:12px 0}.muted{color:#666}pre{background:#111;color:#eee;border-radius:8px;padding:12px;overflow:auto}.ok{color:#087f23}.fail{color:#b00020}</style></head><body>";
    return s;
}

static String option(int value, int current, const char *label)
{
    String s = "<option value='"; s += value; s += "'";
    if (value == current) s += " selected";
    s += ">"; s += label; s += "</option>";
    return s;
}

static void handleStatus()
{
    String s = htmlHeader("MOT Status");
    s += "<h1>Microlino Open Telemetry</h1>";
    s += "<p class='muted'>" MOT_VERSION " · "; s += motDeviceId(); s += "</p>";

    s += "<div class='card'><h2>Live Data</h2>";
    s += "Display CAN: "; s += telemetry.display.valid ? "valid" : "waiting"; s += "<br>";
    s += "SOC: " + String(telemetry.display.soc, 1) + " %<br>";
    s += "Speed: " + String(telemetry.display.speedKmh, 1) + " km/h<br>";
    s += "ODO: " + String(telemetry.display.odometerKm, 1) + " km<br>";
    s += "Range: " + String(telemetry.display.estimatedRangeKm) + " km<br>";
    s += "Charging: " + String(telemetry.charging.isCharging ? "yes" : "no") + "<br>";
    s += "Power: " + String(telemetry.charging.powerDisplay) + "</div>";

    s += "<div class='card'><h2>System</h2>";
    s += "Network: " + networkModeName() + "<br>";
    s += "IP: " + networkIp() + "<br>";
    s += "Vehicle name: " + config.vehicleName + "<br>";
    s += "Vehicle ID: " + config.vehicleId + "<br>";
    s += "MQTT base topic: " + config.mqttPrefix + "/" + config.vehicleId + "<br>";
    s += "Uptime: " + String(millis() / 1000) + " s</div>";


    s += "<div class='card'><h2>System Health</h2>";
    s += "<p class='muted'>Prüft WiFi, DNS, TCP, MQTT und CAN-Status.</p>";
    s += "<button type='button' onclick='loadSystemHealth()'>System Health prüfen</button>";
    s += "<pre id='system-health-result'>Noch nicht geprüft.</pre></div>";
    s += "<script>";
    s += "async function loadSystemHealth(){";
    s += "const out=document.getElementById('system-health-result');out.textContent='Prüfe System Health…';";
    s += "try{const r=await fetch('/api/system-health');const d=await r.json();";
    s += "out.textContent=`Device    : ${d.deviceId||'--'}\\nFirmware  : ${d.firmwareVersion||'--'}\\nBuild     : ${d.buildDate||'--'}\\nIP        : ${d.ip||'--'}\\nRSSI      : ${d.rssi} dBm\\nUptime    : ${d.uptimeText}\\n\\nWiFi      : ${d.wifiOk?'OK':'FAIL'}\\nDNS       : ${d.dnsOk?'OK':'FAIL'}\\nTCP       : ${d.tcpOk?'OK':'FAIL'}\\nMQTT      : ${d.mqttOk?'OK':'FAIL'}\\nCAN       : ${d.canOk?'OK':'WAITING'}\\n\\nMQTT Host : ${d.mqtt.host}\\nMQTT Port : ${d.mqtt.port}\\nMQTT IP   : ${d.mqtt.resolvedIp||'--'}\\nMQTT RC   : ${d.mqtt.mqttState}\\nMessage   : ${d.mqtt.message}\\nDuration  : ${d.mqtt.durationMs} ms`;}";
    s += "catch(e){out.textContent='Fehler beim System-Health-Test: '+e.message;}}";
    s += "</script>";

    s += "<p><a href='/config'>Config</a> · <a href='/update'>OTA Update</a> · <a href='/api/status'>JSON API</a></p></body></html>";
    server.send(200, "text/html", s);
}

static void handleApiStatus()
{
    server.send(200, "application/json", telemetryToJson(telemetry));
}

static void handleConfig()
{
    String s = htmlHeader("MOT Config");
    s += "<h1>Config</h1><form method='POST' action='/save'>";
    s += "<div class='card'><h2>Vehicle</h2>";
    s += "Vehicle name<input name='vehicleName' value='" + config.vehicleName + "'>";
    s += "Device name<input name='deviceName' value='" + config.deviceName + "'>";
    s += "<p class='muted'>Stable device hostname / MQTT client ID. If empty, a stable MAC-based ID is used.</p>";
    s += "Vehicle ID<input name='vehicleId' value='" + config.vehicleId + "'>";
    s += "<p class='muted'>Used in MQTT topics, e.g. mot/pioneer/display/soc. Use lowercase letters, numbers, dash or underscore.</p>";
    s += "MQTT prefix<input name='mqttPrefix' value='" + config.mqttPrefix + "'>";
    s += "<p class='muted'>Usually just mot. The firmware publishes to &lt;prefix&gt;/&lt;vehicleId&gt;/...</p></div>";

    s += "<div class='card'><h2>Network</h2>";
    s += "WiFi SSID<input name='wifiSsid' value='" + config.wifiSsid + "'>";
    s += "WiFi Password<input name='wifiPass' type='password' value='" + config.wifiPass + "'></div>";

    s += "<div class='card'><h2>MQTT</h2>";
    s += "<p class='muted'>MQTT is optional. Leave host empty to disable MQTT without connection errors.</p>";
    s += "MQTT Host<input name='mqttHost' value='" + config.mqttHost + "'>";
    s += "MQTT Port<input name='mqttPort' value='" + String(config.mqttPort) + "'>";
    s += "MQTT User<input name='mqttUser' value='" + config.mqttUser + "'>";
    s += "MQTT Password<input name='mqttPass' type='password' value='" + config.mqttPass + "'>";
    s += "Publish interval ms<input name='pubMs' value='" + String(config.publishIntervalMs) + "'></div>";

    s += "<div class='card'><h2>MQTT Diagnose</h2>";
    s += "<p class='muted'>Testet die gespeicherte MQTT-Konfiguration: WiFi, DNS, TCP-Port und Login.</p>";
    s += "<button type='button' onclick='testMqtt()'>Test Connection</button>";
    s += "<pre id='mqtt-test-result'>Noch nicht geprüft.</pre></div>";
    s += "<script>";
    s += "async function testMqtt(){";
    s += "const out=document.getElementById('mqtt-test-result');out.textContent='Teste MQTT-Verbindung…';";
    s += "try{const r=await fetch('/api/mqtt-test');const d=await r.json();";
    s += "out.textContent=`Host      : ${d.host}\\nPort      : ${d.port}\\nIP        : ${d.resolvedIp||'--'}\\n\\nWiFi      : ${d.wifiConnected?'OK':'FAIL'}\\nDNS       : ${d.dnsOk?'OK':'FAIL'}\\nTCP       : ${d.tcpOk?'OK':'FAIL'}\\nMQTT      : ${d.mqttOk?'OK':'FAIL'}\\n\\nRC        : ${d.mqttState}\\nMessage   : ${d.message}\\nDuration  : ${d.durationMs} ms`;}";
    s += "catch(e){out.textContent='Fehler beim MQTT-Test: '+e.message;}}";
    s += "</script>";


    s += "<div class='card'><h2>CAN</h2>";
    s += "CAN 1 profile<select name='can1Profile'>";
    s += option(0, (int)config.can1Profile, "Microlino Display CAN");
    s += option(1, (int)config.can1Profile, "Microlino CAN (BMS 1 / Pioneer)");
    s += option(2, (int)config.can1Profile, "Microlino CAN (BMS 2 / Standard CAN)");
    s += "</select>";
    s += "CAN 2 profile<select name='can2Profile'>";
    s += option(0, (int)config.can2Profile, "Disabled / unused");
    s += option(1, (int)config.can2Profile, "Microlino CAN (BMS 1 / Pioneer)");
    s += option(2, (int)config.can2Profile, "Microlino CAN (BMS 2 / Standard CAN)");
    s += "</select><p class='muted'>v0.9.x WROOM uses CAN 1 only.</p></div>";

    s += "<div class='card'><h2>OTA / ABRP</h2>";
    s += "<p class='muted'>ABRP is optional. It is only active when API key and user token are both configured.</p>";
    s += "ABRP API Key<input name='abrpApiKey' value='" + config.abrpApiKey + "'>";
    s += "ABRP User Token<input name='abrpToken' value='" + config.abrpUserToken + "'>";
    s += "OTA Password<input name='otaPass' type='password' value='" + config.otaPassword + "'></div>";

    s += "<button type='submit'>Save & Reboot</button></form>";
    s += "<div class='card'><h2>Configuration Management</h2>";
    s += "<p><a href='/api/config/export'>Download config JSON</a></p>";
    s += "<form method='POST' action='/config/import'>";
    s += "<textarea name='configJson' rows='8' style='width:100%;box-sizing:border-box' placeholder='Paste config JSON here'></textarea>";
    s += "<button type='submit'>Import config & reboot</button></form>";
    s += "<p class='muted'>Export contains local secrets such as WiFi and MQTT passwords. Keep it private.</p></div>";
    s += "<div class='card'><h2>Factory Reset</h2><form method='POST' action='/factory-reset'><button type='submit'>Clear config & reboot</button></form></div>";
    s += "<p><a href='/status'>Status</a></p></body></html>";
    server.send(200, "text/html", s);
}

static void handleSave()
{
    config.vehicleName = server.arg("vehicleName");
    if (config.vehicleName.isEmpty()) config.vehicleName = "Microlino Pioneer";

    config.deviceName = server.arg("deviceName");
    config.deviceName.trim();
    config.deviceName.toLowerCase();
    config.deviceName.replace(" ", "-");
    config.deviceName.replace("/", "-");
    if (config.deviceName.isEmpty()) config.deviceName = motHostname();

    config.vehicleId = server.arg("vehicleId");
    config.vehicleId.trim();
    config.vehicleId.toLowerCase();
    config.vehicleId.replace(" ", "-");
    config.vehicleId.replace("/", "-");
    if (config.vehicleId.isEmpty()) config.vehicleId = "pioneer";

    config.mqttPrefix = server.arg("mqttPrefix");
    config.mqttPrefix.trim();
    while (config.mqttPrefix.endsWith("/")) {
        config.mqttPrefix.remove(config.mqttPrefix.length() - 1);
    }
    if (config.mqttPrefix.isEmpty()) config.mqttPrefix = "mot";
    config.wifiSsid = server.arg("wifiSsid");
    config.wifiPass = server.arg("wifiPass");
    config.mqttHost = server.arg("mqttHost");
    config.mqttPort = server.arg("mqttPort").toInt();
    if (config.mqttPort == 0) config.mqttPort = 1883;
    config.mqttUser = server.arg("mqttUser");
    config.mqttPass = server.arg("mqttPass");
    config.publishIntervalMs = server.arg("pubMs").toInt();
    if (config.publishIntervalMs < 1000) config.publishIntervalMs = 5000;
    config.can1Profile = (DecoderProfile)server.arg("can1Profile").toInt();
    config.can2Profile = (DecoderProfile)server.arg("can2Profile").toInt();
    config.abrpApiKey = server.arg("abrpApiKey");
    config.abrpUserToken = server.arg("abrpToken");
    config.otaPassword = server.arg("otaPass");
    saveConfig();
    server.send(200, "text/html", "<!doctype html><html><body style='font-family:sans-serif;text-align:center;margin-top:60px'><h2>Configuration saved.</h2><p>Device will reboot in 5 seconds...</p></body></html>");
    rebootPending = true;
    rebootAtMs = millis() + 5000;
}


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

static void handleFactoryReset()
{
    clearConfig();
    server.send(200, "text/html", "<!doctype html><html><body style='font-family:sans-serif;text-align:center;margin-top:60px'><h2>Configuration cleared.</h2><p>Device will reboot in 5 seconds.</p></body></html>");
    rebootPending = true;
    rebootAtMs = millis() + 5000;
}


static MqttDiagResult runMqttDiagnostics(const char *clientIdPrefix)
{
    return MqttDiagnostics::test(
        config.mqttHost,
        config.mqttPort,
        config.mqttUser,
        config.mqttPass,
        clientIdPrefix
    );
}

static void handleApiMqttTest()
{
    MqttDiagResult result = runMqttDiagnostics("mot-diag");
    server.send(200, "application/json", MqttDiagnostics::toJson(result));
}

static void handleApiSystemHealth()
{
    MqttDiagResult mqtt = runMqttDiagnostics("mot-health");

    SystemHealthResult health;
    health.deviceId = motDeviceId();
    health.firmwareVersion = MOT_VERSION;
    health.buildDate = String(__DATE__) + " " + String(__TIME__);
    health.ip = WiFi.localIP().toString();
    health.rssi = WiFi.RSSI();
    health.uptimeSec = millis() / 1000UL;

    health.wifiOk = WiFi.status() == WL_CONNECTED;
    health.dnsOk = mqtt.dnsOk;
    health.tcpOk = mqtt.tcpOk;
    health.mqttOk = mqtt.mqttOk;
    health.canOk = telemetry.display.valid;
    health.mqtt = mqtt;

    server.send(200, "application/json", SystemHealth::toJson(health));
}

void setupWebUi()
{
    server.on("/", handleStatus);
    server.on("/status", handleStatus);
    server.on("/api/status", handleApiStatus);
    server.on("/api/mqtt-test", handleApiMqttTest);
    server.on("/api/system-health", handleApiSystemHealth);
    server.on("/config", handleConfig);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/api/config/export", HTTP_GET, handleConfigExport);
    server.on("/config/import", HTTP_POST, handleConfigImport);
    server.on("/factory-reset", HTTP_POST, handleFactoryReset);
    server.on("/favicon.ico", []() { server.send(204); });
    setupOtaRoutes(server);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();
    Serial.println("Web UI started");
}

void webUiLoop()
{
    server.handleClient();
    otaWebLoop();
    if (rebootPending && millis() > rebootAtMs) {
        Serial.println("Rebooting now...");
        delay(100);
        ESP.restart();
    }
}
