#include "c6_web.h"

#include <Arduino.h>
#include <WebServer.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "c6_aws.h"
#include "c6_abrp.h"
#include "c6_bm2_diag.h"
#include "c6_config.h"
#include "c6_data_volume.h"
#include "c6_charge_journal.h"
#include "c6_dual_can.h"
#include "c6_gps.h"
#include "c6_journey_energy.h"
#include "c6_local_mqtt.h"
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
    out += "</title><style>body{font-family:system-ui;margin:24px;max-width:820px;background:#f7f8fa;color:#18202b}.card{background:white;border:1px solid #d9dee7;border-radius:12px;padding:16px;margin:12px 0}input,select,textarea{width:100%;padding:9px;margin:4px 0 12px;box-sizing:border-box}button{padding:10px 16px;transition:background-color .15s,color .15s,opacity .15s}.primary{background:#175cd3;color:#fff;border:1px solid #175cd3;border-radius:4px}.secondary-small{background:#e4e7ec;color:#344054;border:1px solid #d0d5dd;border-radius:4px;padding:6px 10px;font-size:.85rem}.wizard-action{display:block;width:240px;max-width:100%;min-height:42px;margin:16px 0 0;text-align:center}button:active,button.is-pending{background:#344054!important;color:#fff!important;opacity:.82;cursor:progress}button.is-pending:after{content:' …'}.compact-control{margin:12px 0 4px 18px;padding-left:12px;border-left:3px solid #d0d5dd}.compact-control label{display:inline-flex;align-items:center;gap:7px;font-size:.95rem}.compact-control input{width:auto;margin:0}.fixed-value{margin:4px 0 12px;padding:9px;background:#f2f4f7;border:1px solid #d0d5dd;border-radius:4px}.muted{color:#667085}pre{white-space:pre-wrap;background:#101828;color:#e4e7ec;padding:12px;border-radius:8px}a{color:#175cd3}</style><script>document.addEventListener('submit',function(e){const f=e.target,b=e.submitter;if(!b||f.dataset.submitting==='1')return;e.preventDefault();b.classList.add('is-pending');b.setAttribute('aria-busy','true');f.dataset.submitting='1';setTimeout(function(){f.requestSubmit(b)},120)})</script></head><body>";
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

String can1ProfileOptions(DecoderProfile current)
{
    String out;
    for (size_t i = 0; i < decoderProfileCount(); ++i) {
        const DecoderProfileDescriptor &profile = decoderProfileAt(i);
        if (!c6ConfigCan1ProfileAllowed(profile.id)) continue;
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
    out += ",\"vehicleSoftware\":{";
    out += "\"vcu\":" + String(telemetry.vehicleSoftware.vcuValid ?
        "\"" + String(telemetry.vehicleSoftware.vcuMajor) + "." +
        String(telemetry.vehicleSoftware.vcuMinor) + "." +
        String(telemetry.vehicleSoftware.vcuBuild) + "\"" : "null");
    out += ",\"ipc\":" + String(telemetry.vehicleSoftware.ipcValid ?
        "\"" + String(telemetry.vehicleSoftware.ipcMajor) + "." +
        String(telemetry.vehicleSoftware.ipcMinor) + "." +
        String(telemetry.vehicleSoftware.ipcPatch) + "\"" : "null");
    out += ",\"bdu\":" + String(telemetry.vehicleSoftware.bduValid ?
        "\"" + String(telemetry.vehicleSoftware.bduMajor) + "." +
        String(telemetry.vehicleSoftware.bduMinor) + "." +
        String(telemetry.vehicleSoftware.bduPatch) + "\"" : "null") + "}";
    const bool gpsMoving = telemetry.display.valid &&
        millis() - telemetry.display.lastUpdateMs <= 5000 &&
        telemetry.display.speedKmh > 1.0f;
    out += ",\"gps\":" + c6GpsStatusJson(gpsMoving);
    out += ",\"aws\":\"" + jsonEscape(c6AwsStatus()) + "\"";
    out += ",\"localMqtt\":" + c6LocalMqttStatusJson();
    out += ",\"offlineCache\":" + c6OfflineCacheStatusJson();
    out += ",\"abrp\":" + c6AbrpStatusJson();
    out += ",\"dataVolume\":" + c6DataVolumeStatusJson();
#if defined(MOT_BM2_DIAGNOSTIC) || defined(MOT_BOARD_NETWORK_BM2)
    out += ",\"bm2\":" + c6Bm2DiagStatusJson();
#endif
    out += "}";
    return out;
}

void statusPage()
{
    if (!requireAdmin()) return;
    String out = header("MOT C6 Status");
    out += "<h1>Microlino Open Telemetry</h1><p>" MOT_VERSION " · " MOT_BOARD " · " + motDeviceId() + "</p>";
    out += "<div class='card'><h2>Vehicle software versions</h2><p>VCU: <b id='sw-vcu'>--</b><br>IPC: <b id='sw-ipc'>--</b><br>BDU: <b id='sw-bdu'>--</b></p><p class='muted'>Passively received CAN broadcasts; no diagnostic request.</p></div>";
    out += "<div class='card'><h2>Runtime diagnostics</h2><pre id='diag'>Loading…</pre><button onclick='load()'>Refresh</button></div>";
#if defined(MOT_BM2_DIAGNOSTIC) || defined(MOT_BOARD_NETWORK_BM2)
    out += "<div class='card'><h2>Battery monitor diagnostics</h2><p>Close the BM2/BM6 smartphone app during this test; the monitor accepts only one BLE connection.</p><pre id='battery-monitor'>Waiting for automatic scan…</pre></div>";
#endif
    out += "<div class='card'><h2>ABRP</h2><pre id='abrp'>Loading…</pre><button onclick='testAbrp()'>Send test telemetry</button></div>";
    out += "<p><a href='/config'>Configuration</a> · <a href='/peripherals'>Local devices</a> · <a href='/wizard'>Onboarding wizard</a> · <a href='/update'>Local OTA</a> · <a href='/api/status'>JSON diagnostics</a></p>";
    out += "<script>async function loadAbrp(){let r=await fetch('/api/abrp/status'),d=await r.json();document.getElementById('abrp').textContent=JSON.stringify(d,null,2)}async function testAbrp(){let r=await fetch('/api/abrp/test',{method:'POST'}),d=await r.json();document.getElementById('abrp').textContent=JSON.stringify(d,null,2)}loadAbrp()</script>";
    out += "<script>async function load(){let r=await fetch('/api/status'),d=await r.json();document.getElementById('diag').textContent=JSON.stringify(d,null,2);let s=d.vehicleSoftware||{};document.getElementById('sw-vcu').textContent=s.vcu||'--';document.getElementById('sw-ipc').textContent=s.ipc||'--';document.getElementById('sw-bdu').textContent=s.bdu||'--';let b=document.getElementById('battery-monitor');if(b)b.textContent=JSON.stringify(d.bm2||{enabled:false},null,2)}load()</script></body></html>";
    server.send(200, "text/html", out);
}

void configPage()
{
    if (!requireAdmin()) return;
    String out = header("MOT C6 Configuration");
    out += "<h1>Configuration</h1><form method='POST' action='/save'>";
    out += "<div class='card'><h2>Preferred home WiFi</h2><label>SSID</label><input name='ssid' maxlength='32' value='" + htmlEscape(c6Config.wifiSsid) + "'><label>Password</label><input type='password' name='wifiPassword' maxlength='63' placeholder='Leave blank to keep current'><h2>Second / mobile hotspot</h2><label>SSID</label><input name='ssid2' maxlength='32' value='" + htmlEscape(c6Config.wifi2Ssid) + "'><label>Password</label><input type='password' name='wifi2Password' maxlength='63' placeholder='Leave blank to keep current'></div>";
    out += "<div class='card'><h2>CAN decoder assignment</h2><label>CAN1</label><select name='can1'>" + can1ProfileOptions(c6Config.can1Profile) + "</select><p class='muted'>CAN1 is the Microlino Standard-CAN input and supports only V1 or V2.</p><label>CAN2</label><select name='can2'>" + profileOptions(c6Config.can2Profile) + "</select></div>";
    if (c6GpsDetected() || !c6Config.gpsEnabled) {
        out += "<div class='card'><h2>GPS</h2><input type='hidden' name='gpsControlPresent' value='1'><label><input style='width:auto' type='checkbox' name='gpsEnabled'" + String(c6Config.gpsEnabled ? " checked" : "") + "> Enable detected GPS module</label><p class='muted'>Default: enabled. Disabling stops GPS initialization, decoding and telemetry after reboot.</p></div>";
    }
    out += "<div class='card'><h2>Telemetry services</h2><h3>MOT Cloud (AWS IoT)</h3><label><input style='width:auto' type='checkbox' name='motCloudEnabled'" + String(c6Config.motCloudEnabled ? " checked" : "") + "> Enable MOT Cloud</label><p class='muted'>The adapter remains AWS ready. Disabling stops cloud connections but retains its provisioned certificates and vehicle assignment.</p><hr><h3>ABRP</h3><label><input style='width:auto' type='checkbox' name='abrpEnabled'" + String(c6Config.abrpEnabled ? " checked" : "") + "> Enable ABRP</label> <label>API key</label><input type='password' name='abrpApiKey' maxlength='192' autocomplete='new-password' placeholder='Leave blank to keep current'><label>User token</label><input type='password' name='abrpUserToken' maxlength='192' autocomplete='new-password' placeholder='Leave blank to keep current'><p class='muted'>ABRP uses WiFi independently and can run with or without MOT Cloud. Credentials are never included in normal backups or diagnostics. Empty fields retain stored values.</p><input type='hidden' name='returnTo' value='/config'><button class='secondary-small' type='submit' formaction='/api/abrp/clear' formmethod='post' onclick=\"return confirm('Delete the stored ABRP API key and user token and disable ABRP?')\">Delete ABRP credentials</button></div>";
    out += "<div class='card'><h2>Secure local MQTT</h2><label><input style='width:auto' type='checkbox' name='localMqttEnabled'" + String(c6Config.localMqttEnabled ? " checked" : "") + "> Enable local MQTT over verified TLS</label><label>Broker hostname</label><input name='localMqttHost' maxlength='253' value='" + htmlEscape(c6Config.localMqttHost) + "' placeholder='mqtt.example.local'><label>TLS port</label><input type='number' min='1' max='65535' name='localMqttPort' value='" + String(c6Config.localMqttPort) + "'><label>Vehicle ID</label><input name='localMqttVehicleId' maxlength='64' value='" + htmlEscape(c6Config.localMqttVehicleId) + "'><label>Topic prefix</label><input name='localMqttTopicPrefix' maxlength='32' value='" + htmlEscape(c6Config.localMqttTopicPrefix) + "'><label>Active envelope interval (ms)</label><input type='number' min='5000' max='3600000' name='mqttPubMs' value='" + String(c6Config.localMqttPublishIntervalMs) + "'><p class='muted'>Default: 60000 ms. Local telemetry is published only as one retained telemetry/state/v1 JSON envelope. Charging and parked states automatically use slower intervals; relevant state changes publish immediately.</p><label>Authentication</label><select name='localMqttAuth'><option value='password'" + String(c6Config.localMqttUseClientCertificate ? "" : " selected") + ">Dedicated username and password</option><option value='mtls'" + String(c6Config.localMqttUseClientCertificate ? " selected" : "") + ">Dedicated client certificate (mTLS)</option></select><label>Broker username</label><input name='localMqttUsername' maxlength='128' autocomplete='off' value='" + htmlEscape(c6Config.localMqttUsername) + "'><label>Broker password</label><input type='password' name='localMqttPassword' maxlength='512' autocomplete='new-password' placeholder='Leave blank to keep stored value'><label>Broker CA / trust anchor (PEM)</label><textarea name='localMqttRootCa' rows='5' maxlength='12288' placeholder='Leave blank to keep stored CA'></textarea><label>Client certificate (PEM, mTLS only)</label><textarea name='localMqttClientCertificate' rows='5' maxlength='12288' placeholder='Leave blank to keep stored certificate'></textarea><label>Client private key (PEM, mTLS only)</label><textarea name='localMqttClientPrivateKey' rows='5' maxlength='8192' placeholder='Leave blank to keep stored private key'></textarea><p class='muted'>TLS server and hostname verification are mandatory. Anonymous and plaintext MQTT are unavailable. Secrets are excluded from diagnostics and backups; blank secret fields retain stored values.</p><pre>" + htmlEscape(c6LocalMqttStatusJson()) + "</pre><input type='hidden' name='returnTo' value='/config'><button class='secondary-small' type='submit' formaction='/api/local-mqtt/clear' formmethod='post' onclick=\"return confirm('Delete all local MQTT credentials and disable local MQTT?')\">Delete local MQTT credentials</button></div>";
    out += "<div class='card'><h2>Local MQTT location sharing</h2><p><b>Current state: " + String(c6Config.localMqttLocationEnabled ? "enabled" : "disabled") + "</b></p><p class='muted'>This is separate from MQTT activation. Opting in includes coordinates in the retained telemetry/state/v1 envelope. Opting out immediately publishes a new envelope without location and removes retained location topics left by older firmware, but cannot erase copies already stored by broker consumers.</p><input type='hidden' name='locationEnabled' value='" + String(c6Config.localMqttLocationEnabled ? "0" : "1") + "'><button class='secondary-small' type='submit' formaction='/api/local-mqtt/location' formmethod='post' onclick=\"return confirm('" + String(c6Config.localMqttLocationEnabled ? "Disable MQTT location sharing and remove location from the retained state?" : "Enable retained MQTT location sharing for this adapter?") + "')\">" + String(c6Config.localMqttLocationEnabled ? "Disable location sharing" : "Enable location sharing") + "</button></div>";
    out += "<div class='card'><h2>Offline cache</h2><label><input style='width:auto' type='checkbox' name='offlineCacheEnabled'" + String(c6Config.offlineCacheEnabled ? " checked" : "") + "> Cache History samples, completed journeys and charges during Internet loss</label><p class='muted'>Default: enabled. SOC/Speed History, compact journey summaries and charge summaries use separate bounded journals under this one setting. Sampling and replay pause while MOT Cloud is disabled; queued records are retained. Trustworthy UTC is required. No GPS/location or WiFi identity is stored. Explicitly disabling the cache or factory reset deletes all journals; physical flash access may still expose unencrypted remnants.</p><pre>" + htmlEscape(c6OfflineCacheStatusJson()) + "</pre><pre>" + htmlEscape(c6JourneyEnergyStatusJson()) + "</pre><pre>" + htmlEscape(c6ChargeJournalStatusJson()) + "</pre></div>";
    out += "<div class='card'><h2>Runtime</h2><label>AWS active envelope interval (ms)</label><input type='number' min='1000' max='3600000' name='pubMs' value='" + String(c6Config.publishIntervalMs) + "'><p class='muted'>Default: 5000 ms while moving. Charging and parked states automatically use slower intervals; relevant state changes publish immediately.</p><label><input style='width:auto' type='checkbox' name='otaEnabled'" + String(c6Config.otaEnabled ? " checked" : "") + "> Enable local OTA</label><label>New admin password</label><input type='password' name='adminPassword' minlength='12' maxlength='63' placeholder='Leave blank to keep current'></div><button class='primary'>Save &amp; reboot</button></form>";
    out += "<div class='card'><h2>Backup / restore</h2><p><a href='/api/config/export'>Download backup (without secrets)</a></p><form method='POST' action='/config/import'><textarea name='configJson' rows='8' placeholder='Paste configuration JSON'></textarea><button>Restore &amp; reboot</button></form></div>";
    out += "<div class='card'><h2>Local BLE devices</h2><p>Assign the 12-V battery monitor locally. The same device-slot model reserves front-left, front-right, rear-left and rear-right for later tyre-pressure sensors.</p><p><a href='/peripherals'>Manage local devices</a></p></div>";
    out += "<div class='card'><h2>Factory reset</h2><form method='POST' action='/factory-reset' onsubmit=\"return confirm('Erase all local configuration?')\"><button>Erase configuration &amp; reboot</button></form></div><p><a href='/status'>Status</a></p></body></html>";
    server.send(200, "text/html", out);
}

void peripheralsPage()
{
    if (!requireAdmin()) return;
    String out = header("MOT Local Devices");
    out += "<h1>Local devices</h1><div class='card'><h2>12-V battery monitor</h2>";
    out += "<p>The pairing window is local, lasts two minutes and does not use MOT Cloud. Close every battery-monitor app first because BM2/BM6 permits only one active BLE connection.</p>";
    out += "<p id='binding'>Loading…</p><button class='primary' id='scan' onclick='startScan()'>Search nearby monitors</button> ";
    out += "<button class='secondary-small' onclick='unbindMonitor()'>Unassign monitor</button><div id='candidates'></div><pre id='result'></pre></div>";
    out += "<div class='card'><h2>Tyre-pressure sensor slots</h2><p>Front left, front right, rear left and rear right are reserved in the local assignment registry. Pairing remains disabled until a real TPMS protocol and stable sensor identity have been qualified.</p></div>";
    out += "<p><a href='/status'>Status</a> · <a href='/config'>Configuration</a></p>";
    out += R"JS(<script>
const result=document.getElementById('result'), candidates=document.getElementById('candidates');
async function request(path,body){const options=body?{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)}:{};const response=await fetch(path,options);const text=await response.text();let data;try{data=JSON.parse(text)}catch(_){data={error:text}}if(!response.ok)throw new Error(data.error||text||response.statusText);return data}
function render(data){const binding=data.binding||{};document.getElementById('binding').textContent=binding.assigned?`Assigned: ${binding.address} (${String(binding.protocol).toUpperCase()})`:'No battery monitor assigned';const pairing=data.pairing||{};candidates.replaceChildren();const title=document.createElement('p');title.textContent=pairing.active?`Pairing active: ${pairing.remainingSeconds}s remaining`:'Pairing inactive';candidates.append(title);for(const item of pairing.candidates||[]){const row=document.createElement('div');row.className='card';const label=document.createElement('span');label.textContent=`${item.name||'Battery monitor'} · ${item.address} · ${String(item.protocol).toUpperCase()} · ${item.rssi} dBm `;const button=document.createElement('button');button.textContent='Assign';button.onclick=()=>bindMonitor(item.address,item.protocol);row.append(label,button);candidates.append(row)}result.textContent=JSON.stringify({connection:data.connectionMessage,automatic:data.autoDiagnostic,history:data.history},null,2)}
async function refresh(){try{render(await request('/api/peripherals/battery-monitor'))}catch(error){result.textContent=error.message}}
async function startScan(){try{render(await request('/api/peripherals/battery-monitor/scan',{}));setTimeout(refresh,16000)}catch(error){result.textContent=error.message}}
async function bindMonitor(address,protocol){if(!confirm(`Assign ${address} as the vehicle battery monitor?`))return;try{render(await request('/api/peripherals/battery-monitor/bind',{address,protocol}));setTimeout(refresh,2000)}catch(error){result.textContent=error.message}}
async function unbindMonitor(){if(!confirm('Unassign the current battery monitor? Existing local history will be cleared.'))return;try{render(await request('/api/peripherals/battery-monitor/unbind',{}))}catch(error){result.textContent=error.message}}
refresh();setInterval(refresh,5000);
</script></body></html>)JS";
    server.send(200, "text/html", out);
}

void batteryMonitorStatus()
{
    if (!requireAdmin()) return;
    server.send(200, "application/json", c6Bm2DiagStatusJson());
}

void batteryMonitorScan()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    const bool started = c6Bm2PairingStart();
    server.send(started ? 202 : 409, "application/json", c6Bm2DiagStatusJson());
}

void batteryMonitorBind()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    String error;
    if (!c6Bm2PairingBind(server.arg("address"), server.arg("protocol"), error)) {
        server.send(409, "application/json", "{\"error\":\"" + jsonEscape(error) + "\"}");
        return;
    }
    server.send(200, "application/json", c6Bm2DiagStatusJson());
}

void batteryMonitorUnbind()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6Bm2PairingUnbind();
    server.send(200, "application/json", c6Bm2DiagStatusJson());
}

void saveConfig()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    String ssid = server.arg("ssid"); ssid.trim();
    String ssid2 = server.arg("ssid2"); ssid2.trim();
    const uint32_t interval = server.arg("pubMs").toInt();
    const uint32_t localMqttInterval = server.arg("mqttPubMs").toInt();
    const DecoderProfile can1 = decoderProfileNormalize(server.arg("can1").toInt());
    const DecoderProfile can2 = decoderProfileNormalize(server.arg("can2").toInt(), DECODER_PROFILE_DISABLED);
    if (!c6ConfigCan1ProfileAllowed(can1)) { server.send(400, "text/plain", "CAN1 profile must be Standard-CAN V1 or V2"); return; }
    if (ssid.length() > 32 || ssid2.length() > 32 || server.arg("wifiPassword").length() > 63 || server.arg("wifi2Password").length() > 63 || interval < 1000 || interval > 3600000 || localMqttInterval < 5000 || localMqttInterval > 3600000) { server.send(400, "text/plain", "Invalid configuration"); return; }
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
    const C6Configuration beforeLocalMqtt = c6Config;
    const bool localMqttEnabled = server.hasArg("localMqttEnabled");
    const uint32_t localMqttPort = server.arg("localMqttPort").toInt();
    const bool hasLocalMqttConfiguration = !server.arg("localMqttHost").isEmpty() ||
        !server.arg("localMqttVehicleId").isEmpty();
    if ((localMqttEnabled || hasLocalMqttConfiguration) &&
        (localMqttPort == 0 || localMqttPort > 65535 ||
         !c6ConfigSetLocalMqttConnection(server.arg("localMqttHost"),
             static_cast<uint16_t>(localMqttPort), server.arg("localMqttVehicleId"),
             server.arg("localMqttTopicPrefix"), server.arg("localMqttUsername"),
             server.arg("localMqttAuth") == "mtls"))) {
        c6Config = beforeLocalMqtt;
        server.send(400, "text/plain", "Invalid secure local MQTT configuration"); return;
    }
    String mqttCredentialError;
    if (!c6LocalMqttStoreCredentials(server.arg("localMqttRootCa"),
            server.arg("localMqttPassword"), server.arg("localMqttClientCertificate"),
            server.arg("localMqttClientPrivateKey"), mqttCredentialError)) {
        c6Config = beforeLocalMqtt;
        server.send(400, "text/plain", "Local MQTT credentials rejected: " + mqttCredentialError); return;
    }
    if (localMqttEnabled && !c6LocalMqttCredentialsReady()) {
        c6Config = beforeLocalMqtt;
        server.send(400, "text/plain", "Local MQTT needs a trust anchor and complete authentication credentials"); return;
    }
    c6Config.localMqttEnabled = localMqttEnabled;
    const bool offlineCacheEnabled = server.hasArg("offlineCacheEnabled");
    String abrpKey = c6Config.abrpApiKey;
    String abrpToken = c6Config.abrpUserToken;
    if (!server.arg("abrpApiKey").isEmpty()) abrpKey = server.arg("abrpApiKey");
    if (!server.arg("abrpUserToken").isEmpty()) abrpToken = server.arg("abrpUserToken");
    if (!c6ConfigSetAbrpCredentials(abrpKey, abrpToken)) { server.send(400, "text/plain", "Invalid ABRP credentials"); return; }
    c6Config.publishIntervalMs = interval;
    c6Config.localMqttPublishIntervalMs = localMqttInterval;
    c6Config.otaEnabled = server.hasArg("otaEnabled");
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
    c6LocalMqttClearCredentials();
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
    const bool settingsStep = step == onboardingStepNumber(OnboardingStep::Hardware) ||
                              step == onboardingStepNumber(OnboardingStep::Connectivity) ||
                              step == onboardingStepNumber(OnboardingStep::Vehicle) ||
                              step == onboardingStepNumber(OnboardingStep::Services) ||
                              step == onboardingStepNumber(OnboardingStep::Validation);
    if (step < onboardingStepCount() && !settingsStep) {
        out += "<button class='primary wizard-action' name='step' value='" + String(step + 1) + "'>Next</button>";
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
            if (c6GpsDetected() || !c6Config.gpsEnabled) out += "<form method='POST' action='/wizard/hardware'><div class='compact-control'><label><input type='checkbox' name='gpsEnabled'" + String(c6Config.gpsEnabled ? " checked" : "") + "> Enable GPS</label></div><button class='primary wizard-action'>Save hardware &amp; continue</button></form>";
            break;
        case OnboardingStep::Connectivity:
            out += "<h2>Connectivity</h2><p>During onboarding the protected hotspot <b>" + htmlEscape(c6NetworkApSsid()) + "</b> remains active at <b>" + localHttpLink("192.168.4.1") + "</b>. After completion it normally becomes inactive while Home or Mobile/WiFi2 is connected and returns when neither network is reachable.</p>";
            out += "<p class='muted'>Use a 2.4 GHz WiFi network. For an iPhone Personal Hotspot, enable <b>Maximize Compatibility</b> so the hotspot offers a compatible 2.4 GHz connection.</p>";
            out += "<p>Current state: <b>" + htmlEscape(c6NetworkStateName()) + "</b> via " + htmlEscape(c6NetworkProfileName()) + "</p>";
            out += "<form method='POST' action='/wizard/connectivity'><label>Home WiFi SSID</label><input name='ssid' maxlength='32' value='" + htmlEscape(c6Config.wifiSsid) + "'><label>Home WiFi password</label><input type='password' name='wifiPassword' maxlength='63' placeholder='Leave blank to keep current'><label>Mobile/WiFi2 SSID</label><input name='ssid2' maxlength='32' value='" + htmlEscape(c6Config.wifi2Ssid) + "'><label>Mobile/WiFi2 password</label><input type='password' name='wifi2Password' maxlength='63' placeholder='Leave blank to keep current'><button class='primary wizard-action'>Save WiFi &amp; continue</button><p class='muted'>These settings are saved now and applied together at the final restart. You can continue through the wizard without restarting.</p></form>";
            break;
        case OnboardingStep::Vehicle:
            out += "<h2>Vehicle and CAN</h2><form method='POST' action='/wizard/vehicle'><label>CAN1</label><select name='can1'>" + can1ProfileOptions(c6Config.can1Profile) + "</select><p class='muted'>Select the Microlino Standard-CAN decoder: V1 or V2.</p><label>CAN2</label><input type='hidden' name='can2' value='" + String(static_cast<int>(DECODER_PROFILE_DISPLAY_CAN)) + "'><div class='fixed-value'><b>Microlino Display CAN</b><br><span class='muted'>Fixed wiring for Microlino use.</span></div><button class='primary wizard-action'>Save CAN &amp; continue</button><p class='muted'>These settings are saved now and applied together at the final restart.</p></form>";
            break;
        case OnboardingStep::Services:
            out += "<h2>Telemetry services</h2><p>AWS: " + htmlEscape(c6AwsStatus()) + "</p><form method='POST' action='/wizard/services'><h3>MOT Cloud (AWS IoT)</h3><label><input style='width:auto' type='checkbox' name='motCloudEnabled'" + String(c6Config.motCloudEnabled ? " checked" : "") + "> Enable MOT Cloud</label><p class='muted'>Default: enabled. The adapter remains AWS ready when disabled; provisioned certificates and vehicle assignment are retained.</p><hr><h3>Offline cache</h3><label><input style='width:auto' type='checkbox' name='offlineCacheEnabled'" + String(c6Config.offlineCacheEnabled ? " checked" : "") + "> Enable offline History and journey cache</label><p class='muted'>Default: enabled. Temporarily stores supported History samples and compact completed journeys while MOT Cloud is enabled and the Internet connection is unavailable. Existing queued records are retained while MOT Cloud is disabled.</p><hr><h3>ABRP (optional)</h3><p class='muted'>A Better Routeplanner integration operates independently over WiFi and is optional. Empty fields retain stored values.</p><label><input style='width:auto' type='checkbox' name='abrpEnabled'" + String(c6Config.abrpEnabled ? " checked" : "") + "> Enable ABRP</label> <label>ABRP API key</label><input type='password' name='abrpApiKey' maxlength='192' placeholder='Leave blank to keep current'><label>ABRP user token</label><input type='password' name='abrpUserToken' maxlength='192' placeholder='Leave blank to keep current'><input type='hidden' name='returnTo' value='/wizard'><button class='primary wizard-action'>Save &amp; continue</button><button class='secondary-small' type='submit' formaction='/api/abrp/clear' formmethod='post' onclick=\"return confirm('Delete the stored ABRP API key and user token and disable ABRP?')\">Delete ABRP credentials</button></form>";
            break;
        case OnboardingStep::Validation:
            out += "<h2>Review and apply</h2><p>Review the settings below. Passwords and service credentials are intentionally not shown.</p><ul><li>Home WiFi: <b>" + (c6Config.wifiSsid.isEmpty() ? String("Not configured") : htmlEscape(c6Config.wifiSsid)) + "</b></li><li>Mobile/WiFi2: <b>" + (c6Config.wifi2Ssid.isEmpty() ? String("Not configured") : htmlEscape(c6Config.wifi2Ssid)) + "</b></li><li>CAN1: <b>" + htmlEscape(decoderProfileName(c6Config.can1Profile)) + "</b></li><li>CAN2: <b>Microlino Display CAN</b></li><li>GPS: <b>" + String(c6Config.gpsEnabled ? "Enabled" : "Disabled") + "</b></li><li>MOT Cloud: <b>" + String(c6Config.motCloudEnabled ? "Enabled" : "Disabled") + "</b></li><li>History cache: <b>" + String(c6Config.offlineCacheEnabled ? "Enabled" : "Disabled") + "</b></li><li>ABRP: <b>" + String(c6Config.abrpEnabled ? "Enabled" : "Disabled") + "</b></li></ul><p><b>One final device restart is required to apply these settings.</b> Your browser will temporarily lose its connection.</p><p>During onboarding, the protected <b>" + htmlEscape(c6NetworkApSsid()) + "</b> hotspot remains available at <b>" + localHttpLink("192.168.4.1") + "</b>.</p><p>After the device restart, reconnect to <b>" + htmlEscape(c6NetworkApSsid()) + "</b>, open <b>" + localHttpLink("192.168.4.1") + "</b>, and sign in as <b>admin</b> to complete device onboarding.</p><p class='muted'>The future WLAN names can be shown now. The actual IP address is assigned by the router only after a real connection and is displayed after the restart.</p><form method='POST' action='/wizard/apply'><button class='primary wizard-action'>Apply settings &amp; restart</button></form>";
            break;
        case OnboardingStep::Finish:
            out += "<h2>Validate and finish</h2><p>The saved hardware, WiFi, CAN and service settings were applied together during the restart. Review the actual connection and runtime state before completing onboarding.</p>" + c6WizardAccessNotice() + "<h2>Device and telemetry validation</h2><p>Start the validation to review device identity, network connection, CAN channels, optional GPS, AWS telemetry and runtime health.</p><p class='muted'>Some telemetry checks can remain inactive until the adapter is connected to the vehicle or AWS credentials have been provisioned.</p><button id='validation-button' type='button' onclick='validateDevice()'>Start validation</button><pre id='validation'>Validation has not been started.</pre><script>async function validateDevice(){const b=document.getElementById('validation-button'),o=document.getElementById('validation');b.classList.add('is-pending');b.setAttribute('aria-busy','true');b.textContent='Validation running';o.textContent='Reading device and telemetry status…';try{let r=await fetch('/api/status'),d=await r.json();o.textContent=JSON.stringify(d,null,2);b.textContent='Validation complete'}catch(e){o.textContent='Validation failed: '+e.message;b.textContent='Try validation again'}finally{b.classList.remove('is-pending');b.removeAttribute('aria-busy')}}</script><form method='POST' action='/api/onboarding/complete'><button class='primary wizard-action'>Complete onboarding</button></form>";
            break;
    }
    out += c6WizardNavigation(step);
    out += "<hr><form method='POST' action='/api/onboarding/restart'><button>Restart wizard</button></form></div><p><a href='/status'>Skip to status</a></p></body></html>";
    server.send(200, "text/html", out);
}

void wizardHardwareSave()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6Config.gpsEnabled = server.hasArg("gpsEnabled");
    c6Config.onboardingStep = 3;
    c6ConfigSave();
    server.sendHeader("Location", "/wizard"); server.send(303);
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
    c6Config.wifiSsid = ssid;
    if (!server.arg("wifiPassword").isEmpty()) c6Config.wifiPassword = server.arg("wifiPassword");
    if (ssid.isEmpty()) c6Config.wifiPassword = "";
    c6Config.wifi2Ssid = ssid2;
    if (!server.arg("wifi2Password").isEmpty()) c6Config.wifi2Password = server.arg("wifi2Password");
    if (ssid2.isEmpty()) c6Config.wifi2Password = "";
    c6Config.onboardingStep = 4;
    c6ConfigSave();
    server.sendHeader("Location", "/wizard"); server.send(303);
}

void wizardVehicleSave()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    const DecoderProfile can1 = decoderProfileNormalize(server.arg("can1").toInt());
    if (!c6ConfigCan1ProfileAllowed(can1)) {
        server.send(400, "text/plain", "CAN1 profile must be Standard-CAN V1 or V2");
        return;
    }
    const DecoderProfile can2 = DECODER_PROFILE_DISPLAY_CAN;
    c6Config.can1Profile = can1;
    c6Config.can2Profile = can2;
    c6Config.onboardingStep = 5;
    c6ConfigSave();
    server.sendHeader("Location", "/wizard"); server.send(303);
}

void wizardServicesSave()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    String abrpKey = c6Config.abrpApiKey;
    String abrpToken = c6Config.abrpUserToken;
    if (!server.arg("abrpApiKey").isEmpty()) abrpKey = server.arg("abrpApiKey");
    if (!server.arg("abrpUserToken").isEmpty()) abrpToken = server.arg("abrpUserToken");
    if (!c6ConfigSetAbrpCredentials(abrpKey, abrpToken)) { server.send(400, "text/plain", "Invalid ABRP credentials"); return; }
    c6Config.motCloudEnabled = server.hasArg("motCloudEnabled");
    c6Config.abrpEnabled = server.hasArg("abrpEnabled");
    c6ConfigSetOfflineCacheEnabled(server.hasArg("offlineCacheEnabled"));
    c6Config.onboardingStep = 6;
    c6ConfigSave();
    server.sendHeader("Location", "/wizard"); server.send(303);
}

void wizardApply()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6Config.onboardingStep = onboardingStepNumber(OnboardingStep::Finish);
    c6ConfigSave();
    server.send(200, "text/html", header("Applying settings") + "<h1>Applying settings</h1><p>The adapter is restarting once to activate the saved hardware, WiFi, CAN and service settings. Your browser will temporarily lose its connection.</p><p>During onboarding, the protected <b>" + htmlEscape(c6NetworkApSsid()) + "</b> hotspot remains available at <b>" + localHttpLink("192.168.4.1") + "</b>.</p><p><b>After the device restart, reconnect to " + htmlEscape(c6NetworkApSsid()) + ", open " + localHttpLink("192.168.4.1") + ", and sign in as admin to complete device onboarding.</b></p></body></html>");
    scheduleReboot();
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

void localMqttClear()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6LocalMqttClearCredentials();
    server.sendHeader("Location", "/config");
    server.send(303);
}

void localMqttLocation()
{
    if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
    c6ConfigSetLocalMqttLocationEnabled(server.arg("locationEnabled") == "1");
    server.sendHeader("Location", "/config");
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
    server.on("/peripherals", HTTP_GET, peripheralsPage);
    server.on("/api/status", HTTP_GET, [] { if (requireAdmin()) server.send(200, "application/json", diagnosticsJson()); });
    server.on("/wizard", HTTP_GET, wizardPage);
    server.on("/wizard/hardware", HTTP_POST, wizardHardwareSave);
    server.on("/wizard/connectivity", HTTP_POST, wizardConnectivitySave);
    server.on("/wizard/vehicle", HTTP_POST, wizardVehicleSave);
    server.on("/wizard/services", HTTP_POST, wizardServicesSave);
    server.on("/wizard/apply", HTTP_POST, wizardApply);
    server.on("/api/onboarding", HTTP_GET, onboardingStatus);
    server.on("/api/onboarding/step", HTTP_POST, onboardingStep);
    server.on("/api/onboarding/complete", HTTP_POST, onboardingComplete);
    server.on("/api/onboarding/restart", HTTP_POST, onboardingRestart);
    server.on("/api/gps/toggle", HTTP_POST, gpsToggle);
    server.on("/api/abrp/status", HTTP_GET, [] { if (requireAdmin()) server.send(200, "application/json", c6AbrpStatusJson()); });
    server.on("/api/data-volume", HTTP_GET, [] { if (requireAdmin()) server.send(200, "application/json", c6DataVolumeStatusJson()); });
    server.on("/api/data-volume/reset", HTTP_POST, [] {
        if (!requireAdmin() || !LocalWebSecurity::requireSameOrigin(server)) return;
        const bool reset = c6DataVolumeReset();
        server.send(reset ? 200 : 409, "application/json", c6DataVolumeStatusJson());
    });
    server.on("/api/abrp/test", HTTP_POST, abrpTest);
    server.on("/api/abrp/clear", HTTP_POST, abrpClear);
    server.on("/api/local-mqtt/status", HTTP_GET, [] { if (requireAdmin()) server.send(200, "application/json", c6LocalMqttStatusJson()); });
    server.on("/api/local-mqtt/clear", HTTP_POST, localMqttClear);
    server.on("/api/local-mqtt/location", HTTP_POST, localMqttLocation);
#if defined(MOT_BM2_DIAGNOSTIC) || defined(MOT_BOARD_NETWORK_BM2)
    server.on("/api/peripherals/battery-monitor", HTTP_GET, batteryMonitorStatus);
    server.on("/api/peripherals/battery-monitor/scan", HTTP_POST, batteryMonitorScan);
    server.on("/api/peripherals/battery-monitor/bind", HTTP_POST, batteryMonitorBind);
    server.on("/api/peripherals/battery-monitor/unbind", HTTP_POST, batteryMonitorUnbind);
#endif
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
