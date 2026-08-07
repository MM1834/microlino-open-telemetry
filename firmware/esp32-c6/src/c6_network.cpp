#include "c6_network.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>

#include "c6_config.h"
#include "system/device_id.h"

namespace {
uint32_t connectStartedMs = 0;
bool apActive = false;
bool mdnsStarted = false;
bool previouslyOnline = false;

String activeApPassword()
{
    return c6ConfigAdminConfigured() ? c6Config.adminPassword : c6Config.setupPassword;
}

void startFallbackAp()
{
    if (apActive) return;
    WiFi.mode(c6Config.wifiSsid.isEmpty() ? WIFI_AP : WIFI_AP_STA);
    const String password = activeApPassword();
    apActive = WiFi.softAP(motFallbackApSsid().c_str(), password.c_str(), 1, false, 4);
    if (!apActive) {
        Serial.println("WiFi AP: failed to start");
        return;
    }
    Serial.printf("WiFi AP: %s at %s, channel=1, WPA2 password=%s\n",
                  motFallbackApSsid().c_str(), WiFi.softAPIP().toString().c_str(),
                  c6ConfigAdminConfigured() ? "local-admin-password" : "device-setup-password");
    if (!c6ConfigAdminConfigured()) {
        Serial.printf("FIRST SETUP AP PASSWORD: %s\n", password.c_str());
    }
}
}

void c6NetworkSetup()
{
    if (c6Config.wifiSsid.isEmpty()) {
        Serial.println("WiFi: not configured; starting protected setup AP");
        startFallbackAp();
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname(motHostname().c_str());
    WiFi.begin(c6Config.wifiSsid.c_str(), c6Config.wifiPassword.c_str());
    connectStartedMs = millis();
    Serial.printf("WiFi: connecting to configured SSID (length=%u)\n",
                  static_cast<unsigned>(c6Config.wifiSsid.length()));
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
}

void c6NetworkLoop()
{
    const bool online = WiFi.status() == WL_CONNECTED;
    if (online) {
        if (!previouslyOnline) {
            if (apActive) {
                WiFi.softAPdisconnect(true);
                WiFi.mode(WIFI_STA);
                apActive = false;
                Serial.println("WiFi: station recovered; fallback AP stopped");
            }
            Serial.printf("WiFi: connected ip=%s rssi=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
            configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
            if (!mdnsStarted) mdnsStarted = MDNS.begin(motHostname().c_str());
        }
        previouslyOnline = true;
        return;
    }
    previouslyOnline = false;
    if (c6Config.wifiSsid.isEmpty()) { startFallbackAp(); return; }
    if (!apActive && millis() - connectStartedMs >= 20000) startFallbackAp();
    // ESP32 auto-reconnect remains active. Calling WiFi.reconnect() while the
    // station is already connecting can disturb the concurrent fallback AP on
    // ESP32-C6 (and produces "sta is connecting" errors).
}

bool c6NetworkOnline() { return WiFi.status() == WL_CONNECTED; }
String c6NetworkIp() { return c6NetworkOnline() ? WiFi.localIP().toString() : String(); }
int c6NetworkRssi() { return c6NetworkOnline() ? WiFi.RSSI() : 0; }

String c6NetworkStatus()
{
    if (c6NetworkOnline()) return "connected ip=" + c6NetworkIp() + " rssi=" + String(c6NetworkRssi()) + " dBm" + (apActive ? " fallback-ap=active" : "");
    if (apActive) return String(c6Config.wifiSsid.isEmpty() ? "setup" : "disconnected") + " fallback-ap=" + motFallbackApSsid() + " ip=" + WiFi.softAPIP().toString();
    return "connecting";
}

bool c6NetworkApActive() { return apActive; }
String c6NetworkApSsid() { return motFallbackApSsid(); }
String c6NetworkApPassword() { return activeApPassword(); }
