#include "web_ui.h"
#include "../app_config.h"
#include "../network/wifi_manager.h"

#include <Arduino.h>
#include <WebServer.h>
#include "telemetry/telemetry.h"
#include "api/telemetry_json.h"
#include "system/device_id.h"
#include "system/version.h"

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
    s += ".card{border:1px solid #ddd;border-radius:12px;padding:16px;margin:12px 0}.muted{color:#666}</style></head><body>";
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
    s += "MQTT prefix: " + config.mqttPrefix + "<br>";
    s += "Uptime: " + String(millis() / 1000) + " s</div>";

    s += "<p><a href='/config'>Config</a> · <a href='/api/status'>JSON API</a></p></body></html>";
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
    s += "MQTT prefix<input name='mqttPrefix' value='" + config.mqttPrefix + "'></div>";

    s += "<div class='card'><h2>Network</h2>";
    s += "WiFi SSID<input name='wifiSsid' value='" + config.wifiSsid + "'>";
    s += "WiFi Password<input name='wifiPass' type='password' value='" + config.wifiPass + "'></div>";

    s += "<div class='card'><h2>MQTT</h2>";
    s += "MQTT Host<input name='mqttHost' value='" + config.mqttHost + "'>";
    s += "MQTT Port<input name='mqttPort' value='" + String(config.mqttPort) + "'>";
    s += "MQTT User<input name='mqttUser' value='" + config.mqttUser + "'>";
    s += "MQTT Password<input name='mqttPass' type='password' value='" + config.mqttPass + "'>";
    s += "Publish interval ms<input name='pubMs' value='" + String(config.publishIntervalMs) + "'></div>";

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
    s += "ABRP API Key<input name='abrpApiKey' value='" + config.abrpApiKey + "'>";
    s += "ABRP User Token<input name='abrpToken' value='" + config.abrpUserToken + "'>";
    s += "OTA Password<input name='otaPass' type='password' value='" + config.otaPassword + "'></div>";

    s += "<button type='submit'>Save & Reboot</button></form>";
    s += "<div class='card'><h2>Factory Reset</h2><form method='POST' action='/factory-reset'><button type='submit'>Clear config & reboot</button></form></div>";
    s += "<p><a href='/status'>Status</a></p></body></html>";
    server.send(200, "text/html", s);
}

static void handleSave()
{
    config.vehicleName = server.arg("vehicleName");
    if (config.vehicleName.isEmpty()) config.vehicleName = "microlino";
    config.mqttPrefix = server.arg("mqttPrefix");
    if (config.mqttPrefix.isEmpty()) config.mqttPrefix = "mot/" + config.vehicleName;
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

static void handleFactoryReset()
{
    clearConfig();
    server.send(200, "text/html", "<!doctype html><html><body style='font-family:sans-serif;text-align:center;margin-top:60px'><h2>Configuration cleared.</h2><p>Device will reboot in 5 seconds.</p></body></html>");
    rebootPending = true;
    rebootAtMs = millis() + 5000;
}

void setupWebUi()
{
    server.on("/", handleStatus);
    server.on("/status", handleStatus);
    server.on("/api/status", handleApiStatus);
    server.on("/config", handleConfig);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/factory-reset", HTTP_POST, handleFactoryReset);
    server.on("/favicon.ico", []() { server.send(204); });
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();
    Serial.println("Web UI started");
}

void webUiLoop()
{
    server.handleClient();
    if (rebootPending && millis() > rebootAtMs) {
        Serial.println("Rebooting now...");
        delay(100);
        ESP.restart();
    }
}
