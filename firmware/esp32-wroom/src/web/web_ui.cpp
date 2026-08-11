#include "web_ui.h"
#include "../app_config.h"
#include "../network/wifi_manager.h"
#include "../ota/ota_web.h"

#include "abrp/wroom_abrp.h"

#include <Arduino.h>
#include <WebServer.h>
#include "telemetry/telemetry.h"
#include "api/telemetry_json.h"
#include "system/device_id.h"
#include "system/version.h"
#include "MqttDiagnostics.h"
#include "SystemHealth.h"
#include "../gps/wroom_gps.h"
#include "../mqtt/mqtt_client.h"
#include "onboarding/onboarding.h"
#include "config/configuration_readiness.h"
#include <WiFi.h>
#include "web/local_web_security.h"

static WebServer server(80);
static bool rebootPending = false;
static unsigned long rebootAtMs = 0;

static bool requireAdmin()
{
    return LocalWebSecurity::authenticate(server, config.otaPassword);
}

static bool requireSameOrigin()
{
    return LocalWebSecurity::requireSameOrigin(server);
}

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

static String profileOptions(DecoderProfile current, bool includeDisabled)
{
    String s;
    for (size_t i = 0; i < decoderProfileCount(); ++i) {
        const DecoderProfileDescriptor &profile = decoderProfileAt(i);
        if (!includeDisabled && profile.id == DECODER_PROFILE_DISABLED) continue;
        s += option((int)profile.id, (int)current, profile.name);
    }
    return s;
}


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

static void handleSetup()
{
    if (config.localAdminConfigured() && !requireAdmin()) return;

    String s = htmlHeader("MOT Local Setup");
    s += "<h1>Local administration setup</h1>";
    s += "<p class='muted'>Required before operational pages, APIs and OTA are available.</p>";
    s += "<form method='POST' action='/setup'>";
    s += "<label>WiFi SSID</label><input name='wifiSsid' value='" + config.wifiSsid + "'>";
    s += "<label>WiFi Password</label><input name='wifiPass' type='password' value='' autocomplete='new-password'>";
    s += "<label>Local admin password (12–63 characters)</label><input name='adminPassword' type='password' minlength='12' maxlength='63' value='' autocomplete='new-password' required>";
    s += "<button type='submit'>Secure device & reboot</button></form></body></html>";
    server.send(200, "text/html", s);
}

static void handleSetupSave()
{
    if (config.localAdminConfigured() && !requireAdmin()) return;
    if (!requireSameOrigin()) return;

    String password = server.arg("adminPassword");
    password.trim();
    if (!isValidLocalAdminPassword(password)) {
        server.send(400, "text/plain", "Local admin password must be 12-63 printable ASCII characters");
        return;
    }

    config.wifiSsid = server.arg("wifiSsid");
    const String wifiPassword = server.arg("wifiPass");
    if (!wifiPassword.isEmpty()) config.wifiPass = wifiPassword;
    config.otaPassword = password;
    config.onboardingComplete = false;
    appConfigManager.save();
    server.send(200, "text/html", "<p>Local administration secured. Device is rebooting.</p>");
    rebootPending = true;
    rebootAtMs = millis() + 1500;
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
    String s = "<div style='display:flex;gap:8px;flex-wrap:wrap;margin-top:18px'>";
    if (step > 1) s += "<a href='/wizard?step=" + String(step - 1) + "'><button type='button'>Back</button></a>";
    if (step < onboardingStepCount()) s += "<a href='/wizard?step=" + String(step + 1) + "'><button type='button'>Next</button></a>";
    s += "<a href='/status'><button type='button'>Skip for now</button></a>";
    s += "</div>";
    return s;
}

static String wizardPage()
{
    const int step = requestedWizardStep();
    OnboardingCapabilities caps{MOT_BOARD, true, false, true, 1};
    String s = htmlHeader("MOT Onboarding");
    s += "<h1>Microlino Open Telemetry</h1><p class='muted'>" MOT_VERSION " · " + String(caps.board) + "</p>";
    s += "<div class='card'><div class='muted'>Step " + String(step) + " of " + String(onboardingStepCount()) + "</div>";
    s += "<progress value='" + String(step) + "' max='" + String(onboardingStepCount()) + "' style='width:100%'></progress>";

    switch (step) {
        case 1:
            s += "<h2>Welcome</h2><p>This assistant guides you through the existing MOT configuration. It does not create a second configuration store.</p>";
            s += "<p>You can leave the wizard at any time and continue later.</p>";
            break;
        case 2:
            s += "<h2>Detected hardware</h2><ul>";
            s += "<li>Board: <b>" + String(caps.board) + "</b></li>";
            s += "<li>WiFi: available</li><li>GPS module: <b id='wizard-gps-hardware'>checking…</b></li><li>CAN1: available</li><li>CAN2: reserved</li>";
            s += "</ul><p class='muted'>GPS is reported as detected only after a checksum-valid NMEA sentence has been received. Runtime checks continue in the validation step.</p>";
            s += "<script>fetch('/api/gps').then(r=>r.json()).then(g=>{const labels={GPS_DISABLED:'disabled',GPS_NOT_DETECTED:'not detected',GPS_DETECTED:'detected',GPS_FIX:'detected with fix'};document.getElementById('wizard-gps-hardware').textContent=labels[g.state]||'unknown';}).catch(()=>{document.getElementById('wizard-gps-hardware').textContent='unknown';});</script>";
            break;
        case 3:
            s += "<h2>Network</h2><p>Configure the WiFi network used outside AP setup mode.</p>";
            s += "<p><a href='/config?return=/wizard?step=3'><button type='button'>Open network configuration</button></a></p>";
            s += "<p class='muted'>Save & Reboot returns the device with the stored network settings.</p>";
            break;
        case 4:
            s += "<h2>Vehicle and CAN profile</h2><p>Set vehicle name, vehicle ID and the decoder profile for CAN1.</p>";
            s += "<p><a href='/config?return=/wizard?step=4'><button type='button'>Open vehicle configuration</button></a></p>";
            s += "<p class='muted'>CAN2 remains reserved and is not enabled by onboarding.</p>";
            break;
        case 5:
            s += "<h2>Telemetry services</h2><p>Enable and configure only the services you use: MQTT, AWS IoT and ABRP.</p>";
            s += "<p><a href='/config?return=/wizard?step=5'><button type='button'>Open service configuration</button></a></p>";
            s += "<p class='muted'>The build determines whether AWS IoT is available. Empty credentials keep optional services inactive.</p>";
            break;
        case 6:
            s += "<h2>Validation</h2><p>Run the existing system-health diagnostics before finishing onboarding.</p>";
            s += "<button type='button' onclick='runWizardValidation()'>Run validation</button><pre id='wizard-validation'>Not checked yet.</pre>";
            s += "<script>async function runWizardValidation(){const o=document.getElementById('wizard-validation');o.textContent='Checking…';try{const r=await fetch('/api/system-health');const d=await r.json();const g=d.gps||{},m=d.mqtt||{},aws=m.mode==='AWS_IOT_X509',transport=aws?'AWS IoT':'Legacy MQTT',transportState=!m.enabled?'DISABLED':(m.mqttOk?'OK':'WAITING');o.textContent=`WiFi: ${d.wifiOk?'OK':'WAITING'}\n${transport}: ${transportState}\nCAN: ${d.canOk?'OK':'WAITING'}\nGPS state: ${g.state||'UNKNOWN'}\nGPS UART: ${g.started?'STARTED':'NOT STARTED'}\nGPS module: ${g.detected?'DETECTED':'NOT DETECTED'}\nGPS UART activity: ${g.seen?(g.detected?'VALID NMEA':'UNVALIDATED / NOISE POSSIBLE'):'NONE'}\nGPS fix: ${g.valid?'VALID':(g.detected?'NO FIX':'N/A')}\n\nOpen the Status page for full diagnostics.`;}catch(e){o.textContent='Validation failed: '+e.message;}}</script>";
            s += "<p><a href='/status?return=/wizard?step=6'>Open full status</a></p>";
            break;
        default:
            s += "<h2>Finish</h2><p>Configuration remains managed by the normal MOT configuration pages. Completing onboarding only disables the automatic wizard launch.</p>";
            s += "<form method='POST' action='/api/onboarding/complete'><button type='submit'>Complete onboarding</button></form>";
            break;
    }

    s += wizardNavigation(step);
    s += "<hr><form method='POST' action='/api/onboarding/restart'><button type='submit'>Restart wizard</button></form></div>";
    s += "</body></html>";
    return s;
}

static void handleWizard() { if (!requireAdmin()) return; server.send(200, "text/html", wizardPage()); }
static void handleOnboardingStatus()
{
    if (!requireAdmin()) return;
    const int step = config.onboardingComplete ? onboardingStepCount() : requestedWizardStep();
    String json = "{\"complete\":" + String(config.onboardingComplete ? "true" : "false") +
                  ",\"step\":\"" + String(onboardingStepId(static_cast<OnboardingStep>(step - 1))) +
                  "\",\"stepNumber\":" + String(step) +
                  ",\"stepCount\":" + String(onboardingStepCount()) +
                  ",\"board\":\"" MOT_BOARD "\",\"wifi\":true,\"lte\":false,\"gps\":" + String(wroomGpsDetected() ? "true" : "false") + ",\"canChannels\":1}";
    server.send(200, "application/json", json);
}
static void handleOnboardingComplete()
{
    if (!requireAdmin()) return;
    if (!requireSameOrigin()) return;
    config.onboardingComplete = true;
    appConfigManager.save();
    server.sendHeader("Location", "/status");
    server.send(303, "text/plain", "");
}
static void handleOnboardingRestart()
{
    if (!requireAdmin()) return;
    if (!requireSameOrigin()) return;
    config.onboardingComplete = false;
    appConfigManager.save();
    server.sendHeader("Location", "/wizard?step=1");
    server.send(303, "text/plain", "");
}

static void handleStatus()
{
    if (!requireAdmin()) return;
    String s = htmlHeader("MOT Status");
    s += "<h1>Microlino Open Telemetry</h1>";
    s += "<p class='muted'>" MOT_VERSION " · "; s += motDeviceId(); s += "</p>";
    const String returnTo = requestedReturnUrl();
    if (!returnTo.isEmpty()) s += "<p><a href='" + returnTo + "'>← Back to onboarding</a></p>";

    s += "<div class='card'><h2>Live Data</h2>";
    s += "CAN profile: "; s += decoderProfileName(config.can1Profile); s += "<br>";
    s += "Decoder: "; s += decoderProfileImplemented(config.can1Profile) ? "active" : "template / no decoded values"; s += "<br>";
    s += "Telemetry: "; s += telemetry.display.valid ? "valid" : "waiting"; s += "<br>";
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
    s += "<p class='muted'>Prüft System, Netzwerk, CAN sowie GPS-Modul, Fix, Position und UTC-Zeit.</p>";
    s += "<button type='button' onclick='loadSystemHealth()'>System Health prüfen</button>";
    s += "<pre id='system-health-result'>Noch nicht geprüft.</pre></div>";
    s += "<script>";
    s += "async function loadSystemHealth(){";
    s += "const out=document.getElementById('system-health-result');out.textContent='Prüfe System Health…';";
    s += "try{const r=await fetch('/api/system-health');const d=await r.json();";
    s += "const g=d.gps||{};const pos=(g.latitude==null||g.longitude==null)?'--':`${Number(g.latitude).toFixed(6)}, ${Number(g.longitude).toFixed(6)}`;const hdop=g.hdop==null?'--':Number(g.hdop).toFixed(2);";
    s += "const m=d.mqtt||{},aws=m.mode==='AWS_IOT_X509',transport=aws?'AWS IoT':'Legacy MQTT',transportState=!m.enabled?'DISABLED':(m.mqttOk?'OK':'FAIL'),activity=g.seen?(g.detected?'VALID NMEA':'UNVALIDATED / NOISE POSSIBLE'):'NONE';";
    s += "out.textContent=`Device    : ${d.deviceId||'--'}\\nFirmware  : ${d.firmwareVersion||'--'}\\nBuild     : ${d.buildDate||'--'}\\nIP        : ${d.ip||'--'}\\nRSSI      : ${d.rssi} dBm\\nUptime    : ${d.uptimeText}\\n\\nWiFi      : ${d.wifiOk?'OK':'FAIL'}\\nTransport : ${transport}\\nConnected : ${transportState}\\nCAN       : ${d.canOk?'OK':'WAITING'}\\n\\nGPS state : ${g.state||'UNKNOWN'}\\nGPS UART  : ${g.started?'STARTED':'NOT STARTED'}\\nGPS module: ${g.detected?'DETECTED':'NOT DETECTED'}\\nUART data : ${activity}\\nGPS fix   : ${g.valid?'VALID':(g.detected?'NO FIX':'N/A')}\\nLocation  : ${pos}\\nSatellites: ${g.satellites??0}\\nHDOP      : ${hdop}\\nFix age   : ${g.ageMs??0} ms\\nGPS UTC   : ${d.utc||'--'}\\n\\nEndpoint  : ${m.host||'--'}\\nPort      : ${m.port||'--'}\\nMQTT RC   : ${m.mqttState}\\nMessage   : ${m.message||'--'}`; }";
    s += "catch(e){out.textContent='Fehler beim System-Health-Test: '+e.message;}}";
    s += "</script>";

    s += "<p><a href='/config'>Config</a> · <a href='/update'>OTA Update</a> · <a href='/api/status'>JSON API</a></p></body></html>";
    server.send(200, "text/html", s);
}

static void handleApiStatus()
{
    if (!requireAdmin()) return;
    server.send(200, "application/json", telemetryToJson(telemetry));
}

static void handleConfig()
{
    if (!requireAdmin()) return;
    const String returnTo = requestedReturnUrl();
    String s = htmlHeader("MOT Config");
    s += "<h1>Config</h1><form method='POST' action='/save'>";
    s += returnField(returnTo);
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
    s += "WiFi Password<input name='wifiPass' type='password' value='' placeholder='Leave blank to keep current'></div>";

    s += "<div class='card'><h2>Services</h2>";
    s += "<label><input style='width:auto' type='checkbox' name='svcAws' value='1'" + String(config.awsServiceEnabled ? " checked" : "") + "> AWS IoT</label><br>";
    s += "<label><input style='width:auto' type='checkbox' name='svcMqtt' value='1'" + String(config.mqttServiceEnabled ? " checked" : "") + "> MQTT</label><br>";
    s += "<label><input style='width:auto' type='checkbox' name='svcAbrp' value='1'" + String(config.abrpServiceEnabled ? " checked" : "") + "> ABRP</label>";
    s += "<p class='muted'>Services are independently configurable. AWS availability depends on the selected build target and provisioned credentials.</p></div>";

    s += "<div class='card'><h2>MQTT</h2>";
    s += "<p class='muted'>MQTT is optional. Leave host empty to disable MQTT without connection errors.</p>";
    s += "MQTT Host<input name='mqttHost' value='" + config.mqttHost + "'>";
    s += "MQTT Port<input name='mqttPort' value='" + String(config.mqttPort) + "'>";
    s += "MQTT User<input name='mqttUser' value='" + config.mqttUser + "'>";
    s += "MQTT Password<input name='mqttPass' type='password' value='' placeholder='Leave blank to keep current'>";
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


    s += "<div class='card'><h2>CAN channels</h2>";
    s += "<h3>CAN 1</h3><p class='ok'>Available · ESP32 TWAI</p>";
    s += "Active CAN profile<select name='can1Profile'>";
    s += profileOptions(config.can1Profile, false);
    s += "</select>";
    s += "<p class='muted'>Display-CAN is the single-CAN default. Standard-CAN V1 - Pioneer decodes the verified battery energy-flow fields; Standard-CAN V2 uses the same provisional layout pending validation on a V2 vehicle.</p>";
    s += "<input type='hidden' name='can2Profile' value='255'>";
    s += "<hr><h3>CAN 2</h3><p class='muted'>Reserved · not available on ESP32-WROOM hardware. Future multi-CAN targets can assign an independent decoder profile here.</p>";
    s += "</div>";

    s += "<div class='card'><h2>OTA / ABRP</h2>";
    s += "<p class='muted'>ABRP is optional. It is only active when API key and user token are both configured.</p>";
    s += "ABRP API Key<input name='abrpApiKey' type='password' value='' placeholder='Leave blank to keep current'>";
    s += "ABRP User Token<input name='abrpToken' type='password' value='' placeholder='Leave blank to keep current'>";
    s += "<label><input style='width:auto' type='checkbox' name='otaEnabled' value='1'" + String(config.otaEnabled ? " checked" : "") + "> Enable local OTA</label><br>";
    s += "Local admin password<input name='otaPass' type='password' minlength='12' maxlength='63' value='' placeholder='Leave blank to keep current'></div>";

    s += "<button type='submit'>Save & Reboot</button></form>";

    s += "<div class='card'><h2>ABRP Status</h2>";
    s += "<p class='muted'>ABRP sends telemetry only when API key and user token are configured.</p>";
    s += "<button type='button' onclick='testAbrp()'>Test ABRP Send</button>";
    s += "<pre id='abrp-status' style='white-space:pre-wrap;margin-top:1rem'>Loading...</pre>";
    s += "<script>";
    s += "async function loadAbrp(){try{const r=await fetch('/api/abrp/status');const d=await r.json();document.getElementById('abrp-status').textContent=`Enabled: ${d.enabled}\\nTime valid: ${d.timeValid}\\nUTC: ${d.utc}\\nLast success: ${d.lastSuccess}\\nHTTP: ${d.lastHttpCode}\\nMessage: ${d.lastMessage}\\nPayload: ${d.lastPayload}`;}catch(e){document.getElementById('abrp-status').textContent=e.message;}}";
    s += "async function testAbrp(){document.getElementById('abrp-status').textContent='Sending test telemetry...';try{const r=await fetch('/api/abrp/test',{method:'POST'});const d=await r.json();document.getElementById('abrp-status').textContent=`Enabled: ${d.enabled}\\nTime valid: ${d.timeValid}\\nUTC: ${d.utc}\\nLast success: ${d.lastSuccess}\\nHTTP: ${d.lastHttpCode}\\nMessage: ${d.lastMessage}\\nPayload: ${d.lastPayload}`;}catch(e){document.getElementById('abrp-status').textContent=e.message;}}";
    s += "loadAbrp();";
    s += "</script></div>";

    s += "<div class='card'><h2>Configuration Management</h2>";
    s += "<p><a href='/api/config/export'>Download config JSON</a></p>";
    s += "<form method='POST' action='/config/import'>";
    s += "<textarea name='configJson' rows='8' style='width:100%;box-sizing:border-box' placeholder='Paste config JSON here'></textarea>";
    s += "<button type='submit'>Import config & reboot</button></form>";
    s += "<p class='muted'>Export excludes passwords and tokens.</p></div>";
    s += "<div class='card'><h2>Factory Reset</h2><form method='POST' action='/factory-reset' onsubmit=\"return confirm('Factory Reset wirklich ausführen? Alle Einstellungen werden gelöscht.');\"><button type='submit'>Clear config & reboot</button></form></div>";
    if (!returnTo.isEmpty()) s += "<p><a href='" + returnTo + "'>← Back to onboarding</a></p>";
    else s += "<p><a href='/status'>Status</a></p>";
    s += "</body></html>";
    server.send(200, "text/html", s);
}

static void handleSave()
{
    if (!requireAdmin()) return;
    if (!requireSameOrigin()) return;
    String requestedAdminPassword = server.arg("otaPass");
    requestedAdminPassword.trim();
    if (!requestedAdminPassword.isEmpty() && !isValidLocalAdminPassword(requestedAdminPassword)) {
        server.send(400, "text/plain", "Local admin password must be 12-63 printable ASCII characters");
        return;
    }
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
    if (!server.arg("wifiPass").isEmpty()) config.wifiPass = server.arg("wifiPass");
    config.awsServiceEnabled = server.hasArg("svcAws");
    config.mqttServiceEnabled = server.hasArg("svcMqtt");
    config.abrpServiceEnabled = server.hasArg("svcAbrp");
    config.mqttHost = server.arg("mqttHost");
    config.mqttPort = server.arg("mqttPort").toInt();
    if (config.mqttPort == 0) config.mqttPort = 1883;
    config.mqttUser = server.arg("mqttUser");
    if (!server.arg("mqttPass").isEmpty()) config.mqttPass = server.arg("mqttPass");
    config.publishIntervalMs = server.arg("pubMs").toInt();
    if (config.publishIntervalMs < 1000) config.publishIntervalMs = 5000;
    config.can1Profile = decoderProfileNormalize(server.arg("can1Profile").toInt());
    config.can2Profile = decoderProfileNormalize(server.arg("can2Profile").toInt(), DECODER_PROFILE_DISABLED);
    if (!server.arg("abrpApiKey").isEmpty()) config.abrpApiKey = server.arg("abrpApiKey");
    if (!server.arg("abrpToken").isEmpty()) config.abrpUserToken = server.arg("abrpToken");
    config.otaEnabled = server.hasArg("otaEnabled");
    if (!requestedAdminPassword.isEmpty()) config.otaPassword = requestedAdminPassword;
    appConfigManager.save();
    const String returnTo = requestedReturnUrl();
    String response = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body style='font-family:sans-serif;text-align:center;margin-top:60px'><h2>Configuration saved.</h2><p>Device will reboot in 5 seconds...</p>";
    if (!returnTo.isEmpty()) {
        response += "<p>Onboarding will resume automatically.</p><p><a href='" + returnTo + "'>Return to onboarding now</a></p>";
        response += "<script>setTimeout(function(){location.href='" + returnTo + "';},8000);</script>";
    }
    response += "</body></html>";
    server.send(200, "text/html", response);
    rebootPending = true;
    rebootAtMs = millis() + 5000;
}



static void handleAbrpStatus()
{
    if (!requireAdmin()) return;
    server.send(200, "application/json", wroomAbrpStatusJson());
}

static void handleAbrpTest()
{
    if (!requireAdmin()) return;
    if (!requireSameOrigin()) return;
    const bool queued = queueWroomAbrpTelemetry();
    server.send(queued ? 202 : 503, "application/json", wroomAbrpStatusJson());
}

static void handleConfigExport()
{
    if (!requireAdmin()) return;
    server.sendHeader("Content-Disposition", "attachment; filename=mot-config.json");
    server.send(200, "application/json", appConfigManager.exportJson(false));
}

static void handleConfigImport()
{
    if (!requireAdmin()) return;
    if (!requireSameOrigin()) return;
    String json = server.arg("configJson");
    if (json.isEmpty()) json = server.arg("plain");

    String error;
    if (!appConfigManager.importJson(json, error)) {
        server.send(400, "text/plain", "Config import failed: " + error);
        return;
    }

    server.send(200, "text/html", "<!doctype html><html><body style='font-family:sans-serif;text-align:center;margin-top:60px'><h2>Configuration imported.</h2><p>Device will reboot in 5 seconds...</p></body></html>");
    rebootPending = true;
    rebootAtMs = millis() + 5000;
}

static void handleFactoryReset()
{
    if (!requireAdmin()) return;
    if (!requireSameOrigin()) return;
    appConfigManager.clear();
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
    if (!requireAdmin()) return;
    MqttDiagResult result = runMqttDiagnostics("mot-diag");
    server.send(200, "application/json", MqttDiagnostics::toJson(result));
}

static void handleApiReadiness()
{
    if (!requireAdmin()) return;
    ConfigurationReadinessInput input;
    input.onboardingComplete = config.onboardingComplete;
    input.networkConfigured = !config.wifiSsid.isEmpty();
    input.networkOnline = WiFi.status() == WL_CONNECTED;
    input.canConfigured = config.can1Profile != DECODER_PROFILE_DISABLED;
    input.canOnline = telemetry.display.valid;
    input.gpsDetected = wroomGpsDetected();
    input.gpsFix = wroomGpsValid();
    input.gpsState = wroomGpsState();
    input.mqttEnabled = config.mqttServiceEnabled;
    input.mqttConfigured = !config.mqttHost.isEmpty() && config.mqttPort > 0;
    input.mqttOnline = mqttTransportConnected();
    input.awsEnabled = config.awsServiceEnabled;
#ifdef MOT_AWS_IOT
    input.awsConfigured = true;
#else
    input.awsConfigured = false;
#endif
    input.abrpEnabled = config.abrpServiceEnabled;
    input.abrpConfigured = !config.abrpApiKey.isEmpty() && !config.abrpUserToken.isEmpty();
    server.send(200, "application/json", ConfigurationReadiness::toJson(input));
}

static void handleApiConfigImport()
{
    if (!requireAdmin()) return;
    if (!requireSameOrigin()) return;
    String json = server.arg("plain");
    if (json.isEmpty()) json = server.arg("configJson");
    String error;
    if (!appConfigManager.importJson(json, error)) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"" + error + "\"}");
        return;
    }
    server.send(200, "application/json", "{\"ok\":true,\"rebootRequired\":true}");
}

static void handleApiSystemHealth()
{
    if (!requireAdmin()) return;
    MqttDiagResult mqtt = mqttTransportDiagnostics();

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
    health.gpsStarted = wroomGpsStarted();
    health.gpsSeen = wroomGpsSeen();
    health.gpsDetected = wroomGpsDetected();
    health.gpsValid = wroomGpsValid();
    health.gpsState = wroomGpsState();
    health.gpsLatitude = wroomGpsLatitude();
    health.gpsLongitude = wroomGpsLongitude();
    health.gpsSatellites = wroomGpsSatellites();
    health.gpsHdop = wroomGpsHdop();
    health.gpsAgeMs = wroomGpsLocationAgeMs();
    health.utc = wroomGpsUtc();
    health.mqtt = mqtt;

    server.send(200, "application/json", SystemHealth::toJson(health));
}

void setupWebUi()
{
    LocalWebSecurity::collectSecurityHeaders(server);
    server.on("/", []() { if (!config.localAdminConfigured()) handleSetup(); else if (config.onboardingComplete) handleStatus(); else handleWizard(); });
    server.on("/setup", HTTP_GET, handleSetup);
    server.on("/setup", HTTP_POST, handleSetupSave);
    server.on("/wizard", HTTP_GET, handleWizard);
    server.on("/api/onboarding", HTTP_GET, handleOnboardingStatus);
    server.on("/api/onboarding/complete", HTTP_POST, handleOnboardingComplete);
    server.on("/api/onboarding/restart", HTTP_POST, handleOnboardingRestart);
    server.on("/status", handleStatus);
    server.on("/api/status", handleApiStatus);
    server.on("/api/mqtt-test", handleApiMqttTest);
    server.on("/api/system-health", handleApiSystemHealth);
    server.on("/api/gps", []() { if (!requireAdmin()) return; server.send(200, "application/json", wroomGpsStatusJson()); });
    server.on("/config", handleConfig);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/api/config", HTTP_GET, handleConfigExport);
    server.on("/api/config", HTTP_POST, handleApiConfigImport);
    server.on("/api/config/export", HTTP_GET, handleConfigExport);
    server.on("/api/config/import", HTTP_POST, handleApiConfigImport);
    server.on("/api/readiness", HTTP_GET, handleApiReadiness);
    server.on("/config/import", HTTP_POST, handleConfigImport);
    server.on("/api/abrp/status", HTTP_GET, handleAbrpStatus);
    server.on("/api/abrp/test", HTTP_POST, handleAbrpTest);
    server.on("/factory-reset", HTTP_POST, handleFactoryReset);
    server.on("/favicon.ico", []() { server.send(204); });
    setupOtaRoutes(server);
    server.onNotFound([]() { if (!requireAdmin()) return; server.send(404, "text/plain", "Not found"); });
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
