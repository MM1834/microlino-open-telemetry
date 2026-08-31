#include "c6_web.h"

#include <Arduino.h>
#include <WebServer.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "c6_aws.h"
#include "c6_abrp.h"
#include "c6_config.h"
#include "c6_dual_can.h"
#include "c6_gps.h"
#include "c6_network.h"
#include "c6_offline_cache.h"
#include "decoders/decoder_profile.h"
#include "system/device_id.h"
#include "system/version.h"
#include "onboarding/onboarding.h"
#include "onboarding/onboarding_ui.h"
#include "telemetry/telemetry.h"
#include "web/local_ota.h"
#include "web/local_web_security.h"

namespace {
WebServer server(80);
LocalOtaOptions otaOptions;
bool rebootPending = false;
uint32_t rebootAtMs = 0;

const char *resetReasonText()
{
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        default: return "unknown";
    }
}

String htmlEscape(String value)
{
    value.replace("&", "&amp;"); value.replace("<", "&lt;");
    value.replace(">", "&gt;"); value.replace("\"", "&quot;"); value.replace("'", "&#39;");
    return value;
}

String jsonEscape(String value)
{
    value.replace("\\", "\\\\"); value.replace("\"", "\\\"");
    value.replace("\n", "\\n"); value.replace("\r", "\\r");
    return value;
}

bool requireAdmin() { return LocalWebSecurity::authenticate(server, c6Config.adminPassword); }

bool requireSetupAccess()
{
    if (c6ConfigAdminConfigured()) return requireAdmin();
    const String initial = c6Config.setupPassword;
    if (!server.authenticate("setup", initial.c_str())) {
        server.requestAuthentication();
        return false;
    }
    return true;
}

String header(const char *title)
{
    String out = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>";
    out += title;
    out += "</title><style>body{font-family:system-ui;margin:24px;max-width:820px;background:#f7f8fa;color:#18202b}.card{background:white;border:1px solid #d9dee7;border-radius:12px;padding:16px;margin:12px 0}input,select,textarea{width:100%;padding:9px;margin:4px 0 12px;box-sizing:border-box}button{padding:10px 16px;transition:background-color .15s,color .15s,opacity .15s}.primary{background:#175cd3;color:#fff;border:1px solid #175cd3;border-radius:4px}.secondary-small{background:#e4e7ec;color:#344054;border:1px solid #d0d5dd;border-radius:4px;padding:6px 10px;font-size:.85rem}button:active,button.is-pending{background:#344054!important;color:#fff!important;opacity:.82;cursor:progress}button.is-pending:after{content:' …'}.compact-control{margin:12px 0 4px 18px;padding-left:12px;border-left:3px solid #d0d5dd}.compact-control label{display:inline-flex;align-items:center;gap:7px;font-size:.95rem}.compact-control input{width:auto;margin:0}.fixed-value{margin:4px 0 12px;padding:9px;background:#f2f4f7;border:1px solid #d0d5dd;border-radius:4px}.muted{color:#667085}pre{white-space:pre-wrap;background:#101828;color:#e4e7ec;padding:12px;border-radius:8px}a{color:#175cd3}</style><script>document.addEventListener('submit',function(e){const f=e.target,b=e.submitter;if(!b||f.dataset.submitting==='1')return;e.preventDefault();b.classList.add('is-pending');b.setAttribute('aria-busy','true');f.dataset.submitting='1';setTimeout(function(){f.requestSubmit(b)},120)})</script></head><body>";
    return out;
}

String localHttpLink(const String &host)
{
    const String safeHost = htmlEscape(host);
    return "<a href='http://" + safeHost + "/'>http://" + safeHost + "/</a>";
}

String profileOptions(DecoderProfile current)
{
    String out;
    for (size_t i = 0; i < decoderProfileCount(); ++i) {
        const DecoderProfileDescriptor &profile = decoderProfileAt(i);
        out += "<option value='" + String(static_cast<int>(profile.id)) + "'";
        if (profile.id == current) out += " selected";
        out += ">" + htmlEscape(profile.name) + "</option>";
    }
    return out;
}

void scheduleReboot(uint32_t delayMs = 2000) { rebootPending = true; rebootAtMs = millis() + delayMs; }

void setupPage()
{
    if (c6ConfigAdminConfigured()) {
        if (!requireAdmin()) return;
        server.sendHeader("Location", "/"); server.send(303);
        return;
    }
    if (!requireSetupAccess()) return;
    String out = header("MOT C6 Setup");
    out += "<h1>Secure local setup</h1><p class='muted'>The setup access and WLAN/WiFi Access Point are protected by the device setup password from the inventory sheet.</p>";
    out += "<form method='POST' action='/setup'><div class='card'><h2>Create local administration access</h2>";
    out += "<label>New local admin / device hotspot password (12–63 characters)</label><input type='password' name='adminPassword' minlength='12' maxlength='63' autocomplete='new-password' required>";
    out += "<label>Repeat local admin / device hotspot password</label><input type='password' name='adminPasswordConfirm' minlength='12' maxlength='63' autocomplete='new-password' required>";
    out += "<p>After saving, this password protects both the <b>admin</b> login and the <b>MOT-xxxx</b> device hotspot. The one-time <b>setup</b> login will no longer work. If the password is lost, it can only be replaced through the physical USB console recovery.</p>";
    out += "<button>Secure device &amp; reboot</button></div></form></body></html>";
    server.send(200, "text/html", out);
}

void setupSave()
{
    if (c6ConfigAdminConfigured()) {
        if (!requireAdmin()) return;
        server.send(409, "text/plain", "Initial setup is already complete; use Configuration");
        return;
    }
    if (!requireSetupAccess() || !LocalWebSecurity::requireSameOrigin(server)) return;
    if (server.arg("adminPassword") != server.arg("adminPasswordConfirm")) {
        server.send(400, "text/html", header("Passwords do not match") + "<h1>Passwords do not match</h1><p>The device was not changed. Go back and enter the same password twice.</p><p><a href='/setup'>Return to setup</a></p></body></html>");
        return;
    }
    if (!c6ConfigSetAdminPassword(server.arg("adminPassword"))) {
        server.send(400, "text/plain", "Admin password must be 12-63 printable ASCII characters"); return;
    }
    c6Config.onboardingComplete = false;
    c6Config.onboardingStep = 1;
    c6Config.otaEnabled = true;
    c6ConfigSave();
    server.send(200, "text/html", header("Setup saved") + "<h1>Setup saved</h1><p>Rebooting. Reconnect to <b>" + htmlEscape(c6NetworkApSsid()) + "</b> using the new password, open <b>" + localHttpLink("192.168.4.1") + "</b>, and sign in as <b>admin</b> with the same new password. The onboarding wizard will continue automatically.</p></body></html>");
    scheduleReboot();
}

String channelJson(size_t channel)
{
    const C6CanChannelStatus &state = c6CanStatus(channel);
    return "{\"started\":" + String(state.started ? "true" : "false") +
           ",\"profile\":\"" + jsonEscape(decoderProfileName(state.profile)) + "\"" +
           ",\"frames\":" + String(state.frames) + ",\"errors\":" + String(state.receiveErrors) +
           ",\"lastFrameAgeMs\":" + String(state.frames ? millis() - state.lastFrameMs : 0) + "}";
}

String diagnosticsJson()
{
    String out = "{\"deviceId\":\"" + motDeviceId() + "\",\"board\":\"" MOT_BOARD "\",\"firmware\":\"" MOT_VERSION "\"";
    out += ",\"uptimeSec\":" + String(millis() / 1000UL);
    out += ",\"runtime\":{\"resetReason\":\"" + String(resetReasonText()) + "\",\"freeHeap\":" + String(ESP.getFreeHeap()) + ",\"minFreeHeap\":" + String(ESP.getMinFreeHeap()) + ",\"largestFreeBlock\":" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) + "}";
    out += ",\"network\":{\"online\":" + String(c6NetworkOnline() ? "true" : "false") + ",\"transportReady\":" + String(c6NetworkTransportReady() ? "true" : "false") + ",\"linkWeak\":" + String(c6NetworkLinkWeak() ? "true" : "false") + ",\"weakForMs\":" + String(c6NetworkWeakForMs()) + ",\"transitions\":" + String(c6NetworkTransitionCount()) + ",\"lastTransitionAgeMs\":" + String(c6NetworkLastTransitionAgeMs()) + ",\"disconnects\":" + String(c6NetworkDisconnectCount()) + ",\"lastDisconnectReason\":" + String(c6NetworkLastDisconnectReason()) + ",\"lastDisconnectName\":\"" + jsonEscape(c6NetworkLastDisconnectReasonName()) + "\",\"lastDisconnectAgeMs\":" + String(c6NetworkLastDisconnectAgeMs()) + ",\"lastDisconnectManagerInitiated\":" + String(c6NetworkLastDisconnectWasManagerInitiated() ? "true" : "false") + ",\"homeConfigured\":" + String(c6NetworkHomeConfigured() ? "true" : "false") + ",\"mobileConfigured\":" + String(c6NetworkMobileConfigured() ? "true" : "false") + ",\"state\":\"" + c6NetworkStateName() + "\",\"profile\":\"" + c6NetworkProfileName() + "\",\"reason\":\"" + jsonEscape(c6NetworkReason()) + "\",\"ip\":\"" + c6NetworkIp() + "\",\"bssid\":\"" + c6NetworkBssid() + "\",\"channel\":" + String(c6NetworkChannel()) + ",\"rssi\":" + String(c6NetworkRssi()) + ",\"apActive\":" + String(c6NetworkApActive() ? "true" : "false") + ",\"apSsid\":\"" + c6NetworkApSsid() + "\"}";
    out += ",\"can1\":" + channelJson(0) + ",\"can2\":" + channelJson(1);
    out += ",\"gps\":{\"state\":\"" + jsonEscape(c6GpsState()) + "\",\"detected\":" + String(c6GpsDetected() ? "true" : "false") + ",\"fix\":" + String(c6GpsValid() ? "true" : "false") + ",\"chars\":" + String(static_cast<unsigned long long>(c6GpsChars())) + ",\"satellites\":" + String(c6GpsSatellites()) + "}";
    out += ",\"aws\":\"" + jsonEscape(c6AwsStatus()) + "\"";
    out += ",\"offlineCache\":" + c6OfflineCacheStatusJson();
    out += ",\"abrp\":" + c6AbrpStatusJson() + "}";
    return out;
}

void statusPage()
{
    if (!requireAdmin()) return;
    String out = header("MOT C6 Status");
    out += "<h1>Microlino Open Telemetry</h1><p>" MOT_VERSION " · " MOT_BOARD " · " + motDeviceId() + "</p>";
    out += "<div class='card'><h2>Runtime diagnostics</h2><pre id='diag'>Loading…</pre><button onclick='load()'>Refresh</button></div>";
    out += "<div class='card'><h2>ABRP</h2><pre id='abrp'>Loading…</pre><button onclick='testAbrp()'>Send test telemetry</button></div>";
    out += "<p><a href='/config'>Configuration</a> · <a href='/wizard'>Onboarding wizard</a> · <a href='/update'>Local OTA</a> · <a href='/api/status'>JSON diagnostics</a></p>";
    out += "<script>async function loadAbrp(){let r=await fetch('/api/abrp/status'),d=await r.json();document.getElementById('abrp').textContent=JSON.stringify(d,null,2)}async function testAbrp(){let r=await fetch('/api/abrp/test',{method:'POST'}),d=await r.json();document.getElementById('abrp').textContent=JSON.stringify(d,null,2)}loadAbrp()</script>";
    out += "<script>async function load(){let r=await fetch('/api/status'),d=await r.json();document.getElementById('diag').textContent=JSON.stringify(d,null,2)}load()</script></body></html>";
    server.send(200, "text/html", out);
}

void configPage()
{
    if (!requireAdmin()) return;
    String out = header("MOT C6 Configuration");
    out += "<h1>Configuration</h1><form method='POST' action='/save'>";
    out += "<div class='card'><h2>Preferred home WiFi</h2><label>SSID</label><input name='ssid' maxlength='32' value='" + htmlEscape(c6Config.wifiSsid) + "'><label>Password</label><input type='password' name='wifiPassword' maxlength='63' placeholder='Leave blank to keep current'><h2>Second / mobile hotspot</h2><label>SSID</label><input name='ssid2' maxlength='32' value='" + htmlEscape(c6Config.wifi2Ssid) + "'><label>Password</label><input type='password' name='wifi2Password' maxlength='63' placeholder='Leave blank to keep current'></div>";
    out += "<div class='card'><h2>CAN decoder assignment</h2><label>CAN1</label><select name='can1'>" + profileOptions(c6Config.can1Profile) + "</select><label>CAN2</label><select name='can2'>" + profileOptions(c6Config.can2Profile) + "</select></div>";
    if (c6GpsDetected() || !c6Config.gpsEnabled) {
        out += "<div class='card'><h2>GPS</h2><input type='hidden' name='gpsControlPresent' value='1'><label><input style='width:auto' type='checkbox' name='gpsEnabled'" + String(c6Config.gpsEnabled ? " checked" : "") + "> Enable detected GPS module</label><p class='muted'>Default: enabled. Disabling stops GPS initialization, decoding and telemetry after reboot.</p></div>";
    }
    out += "<div class='card'><h2>Telemetry services</h2><h3>MOT Cloud (AWS IoT)</h3><label><input style='width:auto' type='checkbox' name='motCloudEnabled'" + String(c6Config.motCloudEnabled ? " checked" : "") + "> Enable MOT Cloud</label><p class='muted'>The adapter remains AWS ready. Disabling stops cloud connections but retains its provisioned certificates and vehicle assignment.</p><hr><h3>ABRP</h3><label><input style='width:auto' type='checkbox' name='abrpEnabled'" + String(c6Config.abrpEnabled ? " checked" : "") + "> Enable ABRP</label> <label>API key</label><input type='password' name='abrpApiKey' maxlength='192' autocomplete='new-password' placeholder='Leave blank to keep current'><label>User token</label><input type='password' name='abrpUserToken' maxlength='192' autocomplete='new-password' placeholder='Leave blank to keep current'><p class='muted'>ABRP uses WiFi independently and can run with or without MOT Cloud. Credentials are never included in normal backups or diagnostics. Empty fields retain stored values.</p><input type='hidden' name='returnTo' value='/config'><button class='secondary-small' type='submit' formaction='/api/abrp/clear' formmethod='post' onclick=\"return confirm('Delete the stored ABRP API key and user token and disable ABRP?')\">Delete ABRP credentials</button></div>";
    out += "<div class='card'><h2>Offline History cache</h2><label><input style='width:auto' type='checkbox' name='offlineCacheEnabled'" + String(c6Config.offlineCacheEnabled ? " checked" : "") + "> Cache SOC and active Speed during Internet loss</label><p class='muted'>Default: disabled. Sampling and replay pause while MOT Cloud is disabled; queued samples are retained. Samples require trustworthy UTC. No GPS/location is stored. Explicitly disabling the cache or factory reset deletes queued samples; physical flash access may still expose unencrypted remnants.</p><pre>" + htmlEscape(c6OfflineCacheStatusJson()) + "</pre></div>";
    out += "<div class='card'><h2>Runtime</h2><label>Telemetry interval (ms)</label><input type='number' min='1000' max='3600000' name='pubMs' value='" + String(c6Config.publishIntervalMs) + "'><label><input style='width:auto' type='checkbox' name='otaEnabled'" + String(c6Config.otaEnabled ? " checked" : "") + "> Enable local OTA</label><label>New admin password</label><input type='password' name='adminPassword' minlength='12' maxlength='63' placeholder='Leave blank to keep current'></div><button class='primary'>Save &amp; reboot</button></form>";
    out += "<div class='card'><h2>Backup / restore</h2><p><a href='/api/config/export'>Download backup (without secrets)</a></p><form method='POST' action='/config/import'><textarea name='configJson' rows='8' placeholder='Paste configuration JSON'></textarea><button>Restore &amp; reboot</button></form></div>";
    out += "<div class='card'><h2>Factory reset</h2><form method='POST' action='/factory-reset' onsubmit=\"return confirm('Erase all local configuration?')\"><button>Erase configuration &amp; reboot</button></form></div><p><a href='/status'>Status</a></p></body></html>";
    server.send(200, "text/html", out);
}

void saveConfig()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    String ssid = server.arg("ssid"); ssid.trim();
    String ssid2 = server.arg("ssid2"); ssid2.trim();
    const uint32_t interval = server.arg("pubMs").toInt();
    const DecoderProfile can1 = decoderProfileNormalize(server.arg("can1").toInt());
    const DecoderProfile can2 = decoderProfileNormalize(server.arg("can2").toInt(), DECODER_PROFILE_DISABLED);
    if (ssid.length() > 32 || ssid2.length() > 32 || server.arg("wifiPassword").length() > 63 || server.arg("wifi2Password").length() > 63 || interval < 1000 || interval > 3600000) { server.send(400, "text/plain", "Invalid configuration"); return; }
    const String newAdmin = server.arg("adminPassword");
    if (!newAdmin.isEmpty() && !c6ConfigSetAdminPassword(newAdmin)) { server.send(400, "text/plain", "Invalid admin password"); return; }
    c6Config.wifiSsid = ssid;
    if (!server.arg("wifiPassword").isEmpty()) c6Config.wifiPassword = server.arg("wifiPassword");
    if (ssid.isEmpty()) c6Config.wifiPassword = "";
    c6Config.wifi2Ssid = ssid2;
    if (!server.arg("wifi2Password").isEmpty()) c6Config.wifi2Password = server.arg("wifi2Password");
    if (ssid2.isEmpty()) c6Config.wifi2Password = "";
    c6Config.can1Profile = can1; c6Config.can2Profile = can2;
    if (server.hasArg("gpsControlPresent")) c6Config.gpsEnabled = server.hasArg("gpsEnabled");
    c6Config.motCloudEnabled = server.hasArg("motCloudEnabled");
    c6Config.abrpEnabled = server.hasArg("abrpEnabled");
    const bool offlineCacheEnabled = server.hasArg("offlineCacheEnabled");
    String abrpKey = c6Config.abrpApiKey;
    String abrpToken = c6Config.abrpUserToken;
    if (!server.arg("abrpApiKey").isEmpty()) abrpKey = server.arg("abrpApiKey");
    if (!server.arg("abrpUserToken").isEmpty()) abrpToken = server.arg("abrpUserToken");
    if (!c6ConfigSetAbrpCredentials(abrpKey, abrpToken)) { server.send(400, "text/plain", "Invalid ABRP credentials"); return; }
    c6Config.publishIntervalMs = interval; c6Config.otaEnabled = server.hasArg("otaEnabled");
    c6ConfigSetOfflineCacheEnabled(offlineCacheEnabled);
    c6ConfigSave();
    server.send(200, "text/html", header("Saved") + "<h1>Configuration saved</h1><p>Rebooting.</p></body></html>");
    scheduleReboot();
}

void exportConfig()
{
    if (!requireAdmin()) return;
    server.sendHeader("Content-Disposition", "attachment; filename=mot-c6-config.json");
    server.send(200, "application/json", c6ConfigExportJson(false));
}

void importConfig()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    String error;
    if (!c6ConfigImportJson(server.arg("configJson"), error)) { server.send(400, "text/plain", "Restore failed: " + error); return; }
    server.send(200, "text/html", header("Restored") + "<h1>Configuration restored</h1><p>Rebooting.</p></body></html>");
    scheduleReboot();
}

void factoryReset()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6ConfigFactoryReset();
    server.send(200, "text/html", header("Reset") + "<h1>Factory reset complete</h1><p>Rebooting into protected setup mode.</p></body></html>");
    scheduleReboot();
}

uint8_t requestedWizardStep()
{
    return onboardingClampStep(server.hasArg("step")
        ? server.arg("step").toInt()
        : c6Config.onboardingStep);
}

String c6WizardNavigation(uint8_t step)
{
    String out = "<form method='POST' action='/api/onboarding/step'><p>";
    if (step > 1) out += "<button name='step' value='" + String(step - 1) + "'>Back</button> ";
    const bool settingsStep = step == onboardingStepNumber(OnboardingStep::Connectivity) ||
                              step == onboardingStepNumber(OnboardingStep::Vehicle) ||
                              step == onboardingStepNumber(OnboardingStep::Services);
    if (step < onboardingStepCount() && !settingsStep) {
        out += "<button name='step' value='" + String(step + 1) + "'>Next</button>";
    }
    out += "</p></form>";
    return out;
}

String c6WizardAccessNotice()
{
    String out = "<div class='card'><h2>Access after setup</h2>";
    if (c6NetworkOnline()) {
        const String profile = c6NetworkProfileName();
        const String activeSsid = profile == "home" ? c6Config.wifiSsid :
                                  profile == "mobile" ? c6Config.wifi2Ssid : String();
        out += "<p>Current WLAN: <b>" + htmlEscape(activeSsid) + "</b> (" +
               htmlEscape(profile == "home" ? "Home" : "Mobile/WiFi2") +
               ")<br>IP: <b>" + localHttpLink(c6NetworkIp()) + "</b></p>";
        out += "<p>After completing onboarding, local access normally uses this WLAN and IP address. The router may assign a different IP later.";
        if (c6Config.motCloudEnabled) out += " After the adapter is successfully linked and online, its current IP is also shown in the MOT Portal.";
        out += "</p>";
    } else {
        out += "<p>No Home or Mobile/WiFi2 connection is currently active.</p>";
    }
    out += "<p>If neither configured WLAN is reachable, the protected hotspot <b>" +
           htmlEscape(c6NetworkApSsid()) + "</b> becomes available. Connect to it, open "
           "<b>" + localHttpLink("192.168.4.1") + "</b>, and sign in as <b>admin</b>.</p></div>";
    return out;
}

void wizardPage()
{
    if (!requireAdmin()) return;
    const uint8_t step = requestedWizardStep();
    String out = header("MOT C6 Onboarding");
    out += "<h1>Local device onboarding</h1><div class='card'>" + onboardingProgress(step);
    switch (static_cast<OnboardingStep>(step - 1)) {
        case OnboardingStep::Welcome:
            out += "<h2>Welcome</h2><p>This local wizard configures the adapter. Cloud account and vehicle assignment remain portal administration tasks.</p>";
            break;
        case OnboardingStep::Hardware:
            out += "<h2>Detected hardware</h2><ul><li>Board: <b>" MOT_BOARD "</b></li><li>WiFi: available</li><li>CAN channels: 2</li><li>GPS: " + htmlEscape(c6GpsState()) + "</li></ul>";
            if (c6GpsDetected() || !c6Config.gpsEnabled) out += "<form class='compact-control' method='POST' action='/api/gps/toggle'><label><input type='checkbox' name='gpsEnabled'" + String(c6Config.gpsEnabled ? " checked" : "") + "> Enable GPS</label> <button class='secondary-small'>Save &amp; reboot</button></form>";
            break;
        case OnboardingStep::Connectivity:
            out += "<h2>Connectivity</h2><p>During onboarding the protected hotspot <b>" + htmlEscape(c6NetworkApSsid()) + "</b> remains active at <b>" + localHttpLink("192.168.4.1") + "</b>. After completion it normally becomes inactive while Home or Mobile/WiFi2 is connected and returns when neither network is reachable.</p>";
            out += "<p class='muted'>Use a 2.4 GHz WiFi network. For an iPhone Personal Hotspot, enable <b>Maximize Compatibility</b> so the hotspot offers a compatible 2.4 GHz connection.</p>";
            out += "<p>Current state: <b>" + htmlEscape(c6NetworkStateName()) + "</b> via " + htmlEscape(c6NetworkProfileName()) + "</p>";
            out += "<form method='POST' action='/wizard/connectivity'><label>Home WiFi SSID</label><input name='ssid' maxlength='32' value='" + htmlEscape(c6Config.wifiSsid) + "'><label>Home WiFi password</label><input type='password' name='wifiPassword' maxlength='63' placeholder='Leave blank to keep current'><label>Mobile/WiFi2 SSID</label><input name='ssid2' maxlength='32' value='" + htmlEscape(c6Config.wifi2Ssid) + "'><label>Mobile/WiFi2 password</label><input type='password' name='wifi2Password' maxlength='63' placeholder='Leave blank to keep current'><button>Save WiFi &amp; continue</button><p class='muted'>The device restarts automatically to apply network changes. Reconnect as admin; the wizard then continues at the next step.</p></form>";
            break;
        case OnboardingStep::Vehicle:
            out += "<h2>Vehicle and CAN</h2><form method='POST' action='/wizard/vehicle'><label>CAN1</label><select name='can1'>" + profileOptions(c6Config.can1Profile) + "</select><label>CAN2</label><input type='hidden' name='can2' value='" + String(static_cast<int>(DECODER_PROFILE_DISPLAY_CAN)) + "'><div class='fixed-value'><b>Microlino Display CAN</b><br><span class='muted'>Fixed wiring for Microlino use.</span></div><button>Save CAN &amp; continue</button><p class='muted'>The device restarts only if a CAN profile was changed. With unchanged settings, the wizard continues immediately.</p></form>";
            break;
        case OnboardingStep::Services:
            out += "<h2>Telemetry services</h2><p>AWS: " + htmlEscape(c6AwsStatus()) + "</p><form method='POST' action='/wizard/services'><h3>MOT Cloud (AWS IoT)</h3><label><input style='width:auto' type='checkbox' name='motCloudEnabled'" + String(c6Config.motCloudEnabled ? " checked" : "") + "> Enable MOT Cloud</label><p class='muted'>Default: enabled. The adapter remains AWS ready when disabled; provisioned certificates and vehicle assignment are retained.</p><hr><h3>History cache</h3><label><input style='width:auto' type='checkbox' name='offlineCacheEnabled'" + String(c6Config.offlineCacheEnabled ? " checked" : "") + "> Enable offline History cache</label><p class='muted'>Temporarily stores supported telemetry while MOT Cloud is enabled and the Internet connection is unavailable. Existing queued samples are retained while MOT Cloud is disabled.</p><hr><h3>ABRP (optional)</h3><p class='muted'>A Better Routeplanner integration operates independently over WiFi and is optional. Empty fields retain stored values.</p><label><input style='width:auto' type='checkbox' name='abrpEnabled'" + String(c6Config.abrpEnabled ? " checked" : "") + "> Enable ABRP</label> <label>ABRP API key</label><input type='password' name='abrpApiKey' maxlength='192' placeholder='Leave blank to keep current'><label>ABRP user token</label><input type='password' name='abrpUserToken' maxlength='192' placeholder='Leave blank to keep current'><input type='hidden' name='returnTo' value='/wizard'><button class='primary'>Save &amp; continue</button> <button class='secondary-small' type='submit' formaction='/api/abrp/clear' formmethod='post' onclick=\"return confirm('Delete the stored ABRP API key and user token and disable ABRP?')\">Delete ABRP credentials</button></form>";
            break;
        case OnboardingStep::Validation:
            out += "<h2>Device and telemetry validation</h2><p>Start the validation to review device identity, network connection, CAN channels, optional GPS, AWS telemetry and runtime health before completing onboarding.</p><p class='muted'>Some telemetry checks can remain inactive until the adapter is connected to the vehicle or AWS credentials have been provisioned.</p><button id='validation-button' type='button' onclick='validateDevice()'>Start validation</button><pre id='validation'>Validation has not been started.</pre><script>async function validateDevice(){const b=document.getElementById('validation-button'),o=document.getElementById('validation');b.classList.add('is-pending');b.setAttribute('aria-busy','true');b.textContent='Validation running';o.textContent='Reading device and telemetry status…';try{let r=await fetch('/api/status'),d=await r.json();o.textContent=JSON.stringify(d,null,2);b.textContent='Validation complete'}catch(e){o.textContent='Validation failed: '+e.message;b.textContent='Try validation again'}finally{b.classList.remove('is-pending');b.removeAttribute('aria-busy')}}</script>";
            break;
        case OnboardingStep::Finish:
            out += "<h2>Finish</h2><p>Completing onboarding disables automatic wizard launch and applies immediately without another restart. A required restart has already occurred at the changed WiFi, CAN or MOT Cloud step. All settings remain editable.</p>" + c6WizardAccessNotice() + "<form method='POST' action='/api/onboarding/complete'><button>Complete onboarding</button></form>";
            break;
    }
    out += c6WizardNavigation(step);
    out += "<hr><form method='POST' action='/api/onboarding/restart'><button>Restart wizard</button></form></div><p><a href='/status'>Skip to status</a></p></body></html>";
    server.send(200, "text/html", out);
}

void onboardingStep()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6Config.onboardingStep = onboardingClampStep(server.arg("step").toInt());
    c6ConfigSave();
    server.sendHeader("Location", "/wizard"); server.send(303);
}

void wizardConnectivitySave()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    String ssid = server.arg("ssid"); ssid.trim();
    String ssid2 = server.arg("ssid2"); ssid2.trim();
    if (ssid.length() > 32 || ssid2.length() > 32 || server.arg("wifiPassword").length() > 63 || server.arg("wifi2Password").length() > 63) { server.send(400, "text/plain", "Invalid WiFi configuration"); return; }
    const bool settingsChanged = ssid != c6Config.wifiSsid || ssid2 != c6Config.wifi2Ssid ||
        (!server.arg("wifiPassword").isEmpty() && server.arg("wifiPassword") != c6Config.wifiPassword) ||
        (!server.arg("wifi2Password").isEmpty() && server.arg("wifi2Password") != c6Config.wifi2Password);
    c6Config.wifiSsid = ssid;
    if (!server.arg("wifiPassword").isEmpty()) c6Config.wifiPassword = server.arg("wifiPassword");
    if (ssid.isEmpty()) c6Config.wifiPassword = "";
    c6Config.wifi2Ssid = ssid2;
    if (!server.arg("wifi2Password").isEmpty()) c6Config.wifi2Password = server.arg("wifi2Password");
    if (ssid2.isEmpty()) c6Config.wifi2Password = "";
    c6Config.onboardingStep = 4;
    c6ConfigSave();
    if (!settingsChanged) {
        server.sendHeader("Location", "/wizard"); server.send(303);
        return;
    }
    server.send(200, "text/html", header("WiFi saved") + "<h1>WiFi saved</h1><p>Rebooting. The protected <b>" + htmlEscape(c6NetworkApSsid()) + "</b> hotspot remains available during onboarding. Reconnect at <b>" + localHttpLink("192.168.4.1") + "</b> and sign in as <b>admin</b>; the wizard will continue automatically.</p></body></html>");
    scheduleReboot();
}

void wizardVehicleSave()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    const DecoderProfile can1 = decoderProfileNormalize(server.arg("can1").toInt());
    const DecoderProfile can2 = DECODER_PROFILE_DISPLAY_CAN;
    const bool profilesChanged = can1 != c6Config.can1Profile || can2 != c6Config.can2Profile;
    c6Config.can1Profile = can1;
    c6Config.can2Profile = can2;
    c6Config.onboardingStep = 5;
    c6ConfigSave();
    if (!profilesChanged) {
        server.sendHeader("Location", "/wizard"); server.send(303);
        return;
    }
    server.send(200, "text/html", header("CAN saved") + "<h1>CAN settings saved</h1><p>Rebooting. Reconnect as <b>admin</b>; onboarding will continue automatically.</p></body></html>");
    scheduleReboot();
}

void wizardServicesSave()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    String abrpKey = c6Config.abrpApiKey;
    String abrpToken = c6Config.abrpUserToken;
    if (!server.arg("abrpApiKey").isEmpty()) abrpKey = server.arg("abrpApiKey");
    if (!server.arg("abrpUserToken").isEmpty()) abrpToken = server.arg("abrpUserToken");
    if (!c6ConfigSetAbrpCredentials(abrpKey, abrpToken)) { server.send(400, "text/plain", "Invalid ABRP credentials"); return; }
    const bool motCloudEnabled = server.hasArg("motCloudEnabled");
    const bool motCloudChanged = motCloudEnabled != c6Config.motCloudEnabled;
    c6Config.motCloudEnabled = motCloudEnabled;
    c6Config.abrpEnabled = server.hasArg("abrpEnabled");
    c6ConfigSetOfflineCacheEnabled(server.hasArg("offlineCacheEnabled"));
    c6Config.onboardingStep = 6;
    c6ConfigSave();
    if (motCloudChanged) {
        server.send(200, "text/html", header("Services saved") + "<h1>Services saved</h1><p>Rebooting to apply the MOT Cloud change. Reconnect as <b>admin</b>; the wizard will continue at validation.</p></body></html>");
        scheduleReboot();
        return;
    }
    server.sendHeader("Location", "/wizard"); server.send(303);
}

void onboardingStatus()
{
    if (!requireAdmin()) return;
    const uint8_t step = c6Config.onboardingComplete ? onboardingStepCount() : requestedWizardStep();
    server.send(200, "application/json", "{\"complete\":" + String(c6Config.onboardingComplete ? "true" : "false") + ",\"step\":" + String(step) + ",\"stepId\":\"" + onboardingStepId(static_cast<OnboardingStep>(step - 1)) + "\",\"stepCount\":" + String(onboardingStepCount()) + "}");
}

void onboardingComplete()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6Config.onboardingComplete = true;
    c6Config.onboardingStep = onboardingStepCount();
    c6ConfigSave();
    server.sendHeader("Location", "/status"); server.send(303);
}

void gpsToggle()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    if (!c6GpsDetected() && c6Config.gpsEnabled) { server.send(404, "text/plain", "GPS module not detected"); return; }
    c6Config.gpsEnabled = server.hasArg("gpsEnabled");
    c6ConfigSave();
    server.send(200, "text/html", header("GPS saved") + "<h1>GPS setting saved</h1><p>Rebooting.</p></body></html>");
    scheduleReboot();
}

void onboardingRestart()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6Config.onboardingComplete = false;
    c6Config.onboardingStep = 1;
    c6ConfigSave();
    c6NetworkEnsureOnboardingAp();
    server.sendHeader("Location", "/wizard"); server.send(303);
}

void abrpTest()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    const bool queued = c6AbrpQueueTelemetry();
    server.send(queued ? 202 : 503, "application/json", c6AbrpStatusJson());
}

void abrpClear()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6ConfigClearAbrpCredentials();
    const String returnTo = server.arg("returnTo") == "/wizard" ? "/wizard" : "/config";
    server.sendHeader("Location", returnTo);
    server.send(303);
}
}

void c6WebSetup()
{
    LocalWebSecurity::collectSecurityHeaders(server);
    server.on("/", HTTP_GET, [] { if (!c6ConfigAdminConfigured()) setupPage(); else if (!c6Config.onboardingComplete) wizardPage(); else statusPage(); });
    server.on("/setup", HTTP_GET, setupPage);
    server.on("/setup", HTTP_POST, setupSave);
    server.on("/status", HTTP_GET, statusPage);
    server.on("/api/status", HTTP_GET, [] { if (requireAdmin()) server.send(200, "application/json", diagnosticsJson()); });
    server.on("/wizard", HTTP_GET, wizardPage);
    server.on("/wizard/connectivity", HTTP_POST, wizardConnectivitySave);
    server.on("/wizard/vehicle", HTTP_POST, wizardVehicleSave);
    server.on("/wizard/services", HTTP_POST, wizardServicesSave);
    server.on("/api/onboarding", HTTP_GET, onboardingStatus);
    server.on("/api/onboarding/step", HTTP_POST, onboardingStep);
    server.on("/api/onboarding/complete", HTTP_POST, onboardingComplete);
    server.on("/api/onboarding/restart", HTTP_POST, onboardingRestart);
    server.on("/api/gps/toggle", HTTP_POST, gpsToggle);
    server.on("/api/abrp/status", HTTP_GET, [] { if (requireAdmin()) server.send(200, "application/json", c6AbrpStatusJson()); });
    server.on("/api/abrp/test", HTTP_POST, abrpTest);
    server.on("/api/abrp/clear", HTTP_POST, abrpClear);
    server.on("/config", HTTP_GET, configPage);
    server.on("/save", HTTP_POST, saveConfig);
    server.on("/api/config/export", HTTP_GET, exportConfig);
    server.on("/config/import", HTTP_POST, importConfig);
    server.on("/factory-reset", HTTP_POST, factoryReset);
    server.on("/favicon.ico", [] { server.send(204); });
    otaOptions.adminPassword = c6Config.adminPassword;
    otaOptions.enabled = c6Config.otaEnabled;
    otaOptions.firmwareLabel = String(MOT_VERSION) + " · " + MOT_BOARD + " · " + motDeviceId();
    localOtaSetup(server, &otaOptions);
    server.onNotFound([] { if (requireAdmin()) server.send(404, "text/plain", "Not found"); });
    server.begin();
    Serial.println("Local authenticated WebUI started");
}

void c6WebLoop()
{
    server.handleClient();
    localOtaLoop();
    if (rebootPending && static_cast<int32_t>(millis() - rebootAtMs) >= 0) ESP.restart();
}
