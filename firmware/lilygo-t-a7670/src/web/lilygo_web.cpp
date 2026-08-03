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
#include "onboarding/onboarding.h"
#include "modem/lilygo_modem.h"
#include "mqtt/lilygo_mqtt.h"
#include "network/lilygo_network.h"
#include "telemetry/telemetry.h"
#include "config/configuration_readiness.h"
#include "lte/lilygo_lte_client.h"

static WebServer server(80);
static bool rebootPending = false;
static unsigned long rebootAtMs = 0;

static String requestedReturnUrl()
{
    if (!server.hasArg("return")) return "";
    String target = server.arg("return");
    target.trim();
    if (!target.startsWith("/wizard")) return "";
    return target;
}

static String returnField(const String &target)
{
    if (target.isEmpty()) return "";
    return "<input type='hidden' name='return' value='" + target + "'>";
}

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
    s += "input,select{padding:9px;width:100%;box-sizing:border-box;margin:6px 0 10px}";
    s += "label{font-weight:600}";
    s += "</style></head><body>";

    return s;
}

static String profileOptions(DecoderProfile current)
{
    String s;
    for (size_t i = 0; i < decoderProfileCount(); ++i) {
        const DecoderProfileDescriptor &profile = decoderProfileAt(i);
        if (profile.id == DECODER_PROFILE_DISABLED) continue;
        s += "<option value='" + String((int)profile.id) + "'";
        if (profile.id == current) s += " selected";
        s += ">" + String(profile.name) + "</option>";
    }
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
    const String returnTo = requestedReturnUrl();
    if (!returnTo.isEmpty()) s += "<p><a href='" + returnTo + "'>← Back to onboarding</a></p>";

    s += "<p><a href='/config'>Config</a> · <a href='/ota'>OTA</a> · <a href='/api/status'>Status JSON</a></p>";

    s += "<div class='card'><h2>Network</h2><button onclick='loadNetwork()'>Refresh</button><pre id='network'>Loading...</pre></div>";
    s += "<div class='card'><h2>LTE Modem</h2><p><a href='/api/lilygo/lte/debug'>LTE Debug</a> · <a href='/api/lilygo/lte/rx-debug'>LTE RX Debug</a> · <a href='/api/lilygo/lte/tcp-test'>LTE TCP Test</a></p><button onclick='loadModem()'>Refresh</button><pre id='modem'>Loading...</pre></div>";
    s += "<div class='card'><h2>GPS Status & Location</h2><p class='muted'>Shows UART state, NMEA reception, fix, coordinates, satellites, HDOP, fix age and GPS UTC.</p><button onclick='loadGps()'>Refresh</button><pre id='gps'>Loading...</pre></div>";
    s += "<div class='card'><h2>CAN Input</h2><button onclick='loadCan()'>Refresh</button><pre id='can'>Loading...</pre><p><a href='/api/lilygo/can/frames'>Latest frames JSON</a></p></div>";
    s += "<div class='card'><h2>Decoded Telemetry</h2><button onclick='loadTelemetry()'>Refresh</button><pre id='telemetry'>Loading...</pre></div>";

    s += "<div class='card'><h2>Telemetry Transport (AWS IoT / Legacy MQTT)</h2>";
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
    const String returnTo = requestedReturnUrl();
    String s = pageHeader("MOT LilyGO Config");

    s += "<h1>Configuration</h1>";

    s += "<form method='POST' action='/config/save'>";
    s += returnField(returnTo);
    s += "<label>Device name</label><input name='deviceName' value='" + lilygoDeviceName() + "'>";
    s += "<label>Vehicle ID</label><input name='vehicleId' value='" + config.vehicleId + "'>";
    s += "<label>MQTT Prefix</label><input name='mqttPrefix' value='" + config.mqttPrefix + "'>";
    s += "<div class='card'><h2>CAN channels</h2><h3>CAN 1</h3><p>Available · ESP32 TWAI</p>";
    s += "<label>Decoder profile</label><select name='canProfile'>" + profileOptions(config.canProfile) + "</select>";
    s += "<p class='muted'>Display CAN is active. Standard CAN is a template and intentionally decodes no values until official PIDs are available.</p><hr><h3>CAN 2</h3><p class='muted'>Reserved · not available on LilyGO T-A7670 hardware.</p></div>";

    s += "<div class='card'><h2>Services</h2>";
    s += "<label><input type='checkbox' style='width:auto' name='svcAws' value='1'" + String(config.awsServiceEnabled ? " checked" : "") + "> AWS IoT</label>";
    s += "<label><input type='checkbox' style='width:auto' name='svcMqtt' value='1'" + String(config.mqttServiceEnabled ? " checked" : "") + "> MQTT</label>";
    s += "<label><input type='checkbox' style='width:auto' name='abrpEnabled' value='1'" + String(config.abrpEnabled ? " checked" : "") + "> ABRP</label>";
    s += "<p class='muted'>Each service is optional. GPS and CAN data are shared telemetry sources.</p></div>";

    s += "<label>WiFi SSID</label><input name='wifiSsid' value='" + config.wifiSsid + "'>";
    s += "<label>WiFi Password</label><input name='wifiPass' type='password' value='" + config.wifiPass + "'>";

    s += "<label>LTE APN</label><input name='lteApn' value='" + config.lteApn + "'>";

    s += "<label>MQTT Host</label><input name='mqttHost' value='" + config.mqttHost + "'>";
    s += "<label>MQTT Port</label><input name='mqttPort' value='" + String(config.mqttPort) + "'>";
    s += "<label>MQTT User</label><input name='mqttUser' value='" + config.mqttUser + "'>";
    s += "<label>MQTT Password</label><input name='mqttPass' type='password' value='" + config.mqttPass + "'>";

    s += "<label>OTA Password</label><input name='otaPassword' type='password' value='" + config.otaPassword + "'>";

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

    if (!returnTo.isEmpty()) s += "<p><a href='" + returnTo + "'>← Back to onboarding</a></p>";
    else s += "<p><a href='/'>Back</a></p>";
    s += "</body></html>";

    server.send(200, "text/html", s);
}

static void handleConfigSave()
{
    config.deviceName = server.arg("deviceName");
    config.vehicleId = server.arg("vehicleId");
    config.mqttPrefix = server.arg("mqttPrefix");
    config.canProfile = decoderProfileNormalize(server.arg("canProfile").toInt());

    config.wifiSsid = server.arg("wifiSsid");
    config.wifiPass = server.arg("wifiPass");

    config.lteApn = server.arg("lteApn");

    config.awsServiceEnabled = server.hasArg("svcAws");
    config.mqttServiceEnabled = server.hasArg("svcMqtt");
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

    lilygoConfigManager.save();

    const String returnTo = requestedReturnUrl();
    String response = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body style='font-family:sans-serif;text-align:center;margin-top:60px'><h2>Configuration saved.</h2><p>Device is rebooting...</p>";
    if (!returnTo.isEmpty()) {
        response += "<p>Onboarding will resume automatically.</p><p><a href='" + returnTo + "'>Return to onboarding now</a></p>";
        response += "<script>setTimeout(function(){location.href='" + returnTo + "';},4000);</script>";
    }
    response += "</body></html>";
    server.send(200, "text/html", response);
    rebootPending = true;
    rebootAtMs = millis() + 1000;
}

static bool importRequestConfig(String& error)
{
    String body = server.hasArg("configJson") ? server.arg("configJson") : server.arg("plain");
    body.trim();
    if (body.isEmpty()) {
        error = "empty config JSON";
        return false;
    }
    return lilygoConfigManager.importJson(body, error);
}

static void handleConfigImport()
{
    String error;
    if (!importRequestConfig(error)) {
        server.send(400, "text/plain", "Config import failed: " + error);
        return;
    }
    server.send(200, "text/html", "<p>Config restored. Rebooting...</p>");
    rebootPending = true;
    rebootAtMs = millis() + 1000;
}

static void handleApiConfigImport()
{
    String error;
    if (!importRequestConfig(error)) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"" + error + "\"}");
        return;
    }
    server.send(200, "application/json", "{\"ok\":true,\"rebootRequired\":true}");
}

static void handleFactoryReset()
{
    lilygoConfigManager.clear();

    server.send(200, "text/html", "<p>Config cleared. Rebooting...</p>");
    rebootPending = true;
    rebootAtMs = millis() + 1000;
}

static void handleConfigExport()
{
    server.sendHeader("Content-Disposition", "attachment; filename=mot-lilygo-config.json");
    server.send(200, "application/json", lilygoConfigManager.exportJson(true));
}

static void handleReadiness()
{
    ConfigurationReadinessInput input;
    input.onboardingComplete = config.onboardingComplete;
    input.networkConfigured = !config.wifiSsid.isEmpty() || !config.lteApn.isEmpty();
    input.networkOnline = lilygoNetworkOnline();
    input.canConfigured = config.canProfile != DECODER_PROFILE_DISABLED;
    input.canOnline = lilygoCanReady();
    input.gpsDetected = l76kGpsDetected();
    input.gpsFix = l76kGpsValid();
    input.gpsState = l76kGpsStateName();
    input.mqttEnabled = config.mqttServiceEnabled;
    input.mqttConfigured = !config.mqttHost.isEmpty() && config.mqttPort > 0;
    input.mqttOnline = lilygoMqttConnected();
    input.awsEnabled = config.awsServiceEnabled;
#ifdef MOT_AWS_IOT
    input.awsConfigured = true;
#else
    input.awsConfigured = false;
#endif
    input.abrpEnabled = config.abrpEnabled;
    input.abrpConfigured = !config.abrpApiKey.isEmpty() && !config.abrpUserToken.isEmpty();
    server.send(200, "application/json", ConfigurationReadiness::toJson(input));
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



static void handleLteMqttTrace()
{
    server.send(200, "application/json", lilygoLteClientTraceJson());
}

static void handleLteMqttTraceClear()
{
    lilygoLteClientTraceClear();
    server.send(200, "application/json", lilygoLteClientTraceJson());
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


static int requestedWizardStep()
{
    int step = server.hasArg("step") ? server.arg("step").toInt() : 1;
    if (step < 1) step = 1;
    if (step > onboardingStepCount()) step = onboardingStepCount();
    return step;
}

static String wizardNavigation(int step)
{
    String html = "<div style='display:flex;gap:8px;flex-wrap:wrap;margin-top:18px'>";
    if (step > 1) html += "<a href='/wizard?step=" + String(step - 1) + "'><button type='button'>Back</button></a>";
    if (step < onboardingStepCount()) html += "<a href='/wizard?step=" + String(step + 1) + "'><button type='button'>Next</button></a>";
    html += "<a href='/?skip=1'><button type='button'>Skip for now</button></a></div>";
    return html;
}

static void handleWizard()
{
    const int step = requestedWizardStep();
    String html = pageHeader("MOT Onboarding");
    html += "<h1>Microlino Open Telemetry</h1><p class='muted'>" MOT_VERSION " · " MOT_BOARD "</p>";
    html += "<div class='card'><div class='muted'>Step " + String(step) + " of " + String(onboardingStepCount()) + "</div>";
    html += "<progress value='" + String(step) + "' max='" + String(onboardingStepCount()) + "' style='width:100%'></progress>";

    switch (step) {
        case 1:
            html += "<h2>Welcome</h2><p>This assistant guides you through the existing MOT configuration. It does not create a second configuration store.</p>";
            html += "<p>You can leave the wizard at any time and continue later.</p>";
            break;
        case 2:
            html += "<h2>Detected hardware</h2><ul><li>Board: <b>" MOT_BOARD "</b></li><li>WiFi: available</li><li>LTE modem: available</li><li>GPS module: <span id=\"wizard-gps-hardware\">checking…</span></li><li>CAN1: available</li><li>CAN2: reserved</li></ul>";
            html += "<script>fetch('/api/lilygo/gps').then(r=>r.json()).then(g=>{const labels={GPS_DISABLED:'disabled',GPS_NOT_DETECTED:'not detected',GPS_DETECTED:'detected',GPS_FIX:'detected with fix'};document.getElementById('wizard-gps-hardware').textContent=labels[g.state]||'unknown';}).catch(()=>{document.getElementById('wizard-gps-hardware').textContent='unknown';});</script>";
            html += "<p class='muted'>Runtime checks are performed in the validation step.</p>";
            break;
        case 3:
            html += "<h2>Connectivity</h2><p>Configure WiFi and the mobile-network parameters required by your SIM.</p><p><a href='/config?return=/wizard?step=3'><button type='button'>Open connectivity configuration</button></a></p>";
            break;
        case 4:
            html += "<h2>Vehicle and CAN profile</h2><p>Set the vehicle ID and select the decoder profile used by CAN1.</p><p><a href='/config?return=/wizard?step=4'><button type='button'>Open vehicle configuration</button></a></p>";
            break;
        case 5:
            html += "<h2>Telemetry services</h2><p>Configure MQTT and ABRP using the existing service settings.</p><p><a href='/config?return=/wizard?step=5'><button type='button'>Open service configuration</button></a></p>";
            break;
        case 6:
            html += "<h2>Validation</h2><p>Read the live LilyGO diagnostics before completing onboarding.</p>";
            html += "<button type='button' onclick='runWizardValidation()'>Run validation</button><pre id='wizard-validation'>Not checked yet.</pre>";
            html += "<script>async function getj(u){const r=await fetch(u);return await r.json()}async function runWizardValidation(){const o=document.getElementById('wizard-validation');o.textContent='Checking…';try{const n=await getj('/api/lilygo/network'),m=await getj('/api/lilygo/modem'),g=await getj('/api/lilygo/gps'),c=await getj('/api/lilygo/can'),q=await getj('/api/lilygo/mqtt');o.textContent=`Network:\n${JSON.stringify(n,null,2)}\n\nModem:\n${JSON.stringify(m,null,2)}\n\nGPS:\n${JSON.stringify(g,null,2)}\n\nCAN:\n${JSON.stringify(c,null,2)}\n\nTelemetry transport:\n${JSON.stringify(q,null,2)}`;}catch(e){o.textContent='Validation failed: '+e.message;}}</script>";
            html += "<p><a href='/?skip=1&return=/wizard?step=6'>Open full status</a></p>";
            break;
        default:
            html += "<h2>Finish</h2><p>Completing onboarding disables automatic wizard launch. All settings remain editable in the normal configuration page.</p>";
            html += "<form method='POST' action='/api/onboarding/complete'><button type='submit'>Complete onboarding</button></form>";
            break;
    }

    html += wizardNavigation(step);
    html += "<hr><form method='POST' action='/api/onboarding/restart'><button type='submit'>Restart wizard</button></form></div></body></html>";
    server.send(200, "text/html", html);
}

static void handleOnboardingStatus()
{
    const int step = config.onboardingComplete ? onboardingStepCount() : requestedWizardStep();
    String json = "{\"complete\":" + String(config.onboardingComplete ? "true" : "false") +
                  ",\"step\":\"" + String(onboardingStepId(static_cast<OnboardingStep>(step - 1))) +
                  "\",\"stepNumber\":" + String(step) +
                  ",\"stepCount\":" + String(onboardingStepCount()) +
                  ",\"board\":\"" MOT_BOARD "\",\"wifi\":true,\"lte\":true,\"gps\":" + String(l76kGpsDetected() ? "true" : "false") + ",\"gpsState\":\"" + String(l76kGpsStateName()) + "\",\"canChannels\":1}";
    server.send(200, "application/json", json);
}

static void handleOnboardingComplete()
{
    config.onboardingComplete = true;
    lilygoConfigManager.save();
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
}

static void handleOnboardingRestart()
{
    config.onboardingComplete = false;
    lilygoConfigManager.save();
    server.sendHeader("Location", "/wizard?step=1");
    server.send(303, "text/plain", "");
}

void setupLilygoWeb()
{
    server.on("/", HTTP_GET, []() { if (config.onboardingComplete || server.hasArg("skip")) handleRoot(); else handleWizard(); });
    server.on("/wizard", HTTP_GET, handleWizard);
    server.on("/api/onboarding", HTTP_GET, handleOnboardingStatus);
    server.on("/api/onboarding/complete", HTTP_POST, handleOnboardingComplete);
    server.on("/api/onboarding/restart", HTTP_POST, handleOnboardingRestart);
    server.on("/config", HTTP_GET, handleConfig);
    server.on("/config/save", HTTP_POST, handleConfigSave);
    server.on("/config/import", HTTP_POST, handleConfigImport);
    server.on("/factory-reset", HTTP_POST, handleFactoryReset);

    server.on("/api/config", HTTP_GET, handleConfigExport);
    server.on("/api/config", HTTP_POST, handleApiConfigImport);
    server.on("/api/config/export", HTTP_GET, handleConfigExport);
    server.on("/api/config/import", HTTP_POST, handleApiConfigImport);
    server.on("/api/readiness", HTTP_GET, handleReadiness);
    server.on("/api/status", HTTP_GET, handleStatusJson);

    server.on("/api/lilygo/network", HTTP_GET, handleNetwork);
    server.on("/api/lilygo/lte/debug", HTTP_GET, handleLteDebug);
    server.on("/api/lilygo/lte/rx-debug", HTTP_GET, handleLteRxDebug);
    server.on("/api/lilygo/lte/tcp-test", HTTP_GET, handleLteTcpTest);
    server.on("/api/lilygo/lte/tcp-test", HTTP_POST, handleLteTcpTest);
    server.on("/api/lilygo/abrp", HTTP_GET, handleAbrp);
    server.on("/api/lilygo/abrp/test", HTTP_POST, handleAbrpTest);
    server.on("/api/lilygo/mqtt", HTTP_GET, handleMqtt);
    server.on("/api/lilygo/lte/mqtt-trace", HTTP_GET, handleLteMqttTrace);
    server.on("/api/lilygo/lte/mqtt-trace/clear", HTTP_POST, handleLteMqttTraceClear);
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
