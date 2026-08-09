#include "c6_network.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

#include "c6_config.h"
#include "system/device_id.h"

namespace {
enum class WifiProfile : uint8_t { NONE, HOME, MOBILE };

constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t RETRY_INTERVAL_MS = 30000;
constexpr uint32_t HOME_SCAN_INTERVAL_MS = 60000;
constexpr uint32_t STABLE_INTERVAL_MS = 10000;

WifiProfile targetProfile = WifiProfile::NONE;
WifiProfile activeProfile = WifiProfile::NONE;
uint32_t attemptStartedMs = 0;
uint32_t connectedSinceMs = 0;
uint32_t nextRetryMs = 0;
uint32_t lastHomeScanMs = 0;
bool attempting = false;
bool scanRunning = false;
bool apActive = false;
bool mdnsStarted = false;
bool previouslyOnline = false;
String lastReason = "startup";

bool stationOnline()
{
    return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

bool configured(WifiProfile profile)
{
    return profile == WifiProfile::HOME ? !c6Config.wifiSsid.isEmpty() :
           profile == WifiProfile::MOBILE ? !c6Config.wifi2Ssid.isEmpty() : false;
}

const String &ssid(WifiProfile profile)
{
    return profile == WifiProfile::HOME ? c6Config.wifiSsid : c6Config.wifi2Ssid;
}

const String &password(WifiProfile profile)
{
    return profile == WifiProfile::HOME ? c6Config.wifiPassword : c6Config.wifi2Password;
}

const char *profileName(WifiProfile profile)
{
    if (profile == WifiProfile::HOME) return "home";
    if (profile == WifiProfile::MOBILE) return "mobile";
    return "none";
}

String activeApPassword()
{
    return c6ConfigAdminConfigured() ? c6Config.adminPassword : c6Config.setupPassword;
}

void startFallbackAp()
{
    if (apActive) return;
    WiFi.mode(configured(WifiProfile::HOME) || configured(WifiProfile::MOBILE) ? WIFI_AP_STA : WIFI_AP);
    const String value = activeApPassword();
    apActive = WiFi.softAP(motFallbackApSsid().c_str(), value.c_str(), 1, false, 4);
    if (!apActive) {
        lastReason = "fallback AP start failed";
        Serial.println("WiFi AP: failed to start");
        return;
    }
    Serial.printf("WiFi AP: %s at %s, WPA2=%s\n", motFallbackApSsid().c_str(),
                  WiFi.softAPIP().toString().c_str(),
                  c6ConfigAdminConfigured() ? "local-admin" : "device-setup");
    if (!c6ConfigAdminConfigured()) Serial.printf("FIRST SETUP AP PASSWORD: %s\n", value.c_str());
}

void stopFallbackAp()
{
    if (!apActive) return;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apActive = false;
    Serial.println("WiFi: stable station; fallback AP stopped");
}

void beginProfile(WifiProfile profile, const char *reason)
{
    if (!configured(profile)) return;
    if (scanRunning) { WiFi.scanDelete(); scanRunning = false; }
    WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
    WiFi.disconnect(false, false);
    WiFi.begin(ssid(profile).c_str(), password(profile).c_str());
    targetProfile = profile;
    activeProfile = WifiProfile::NONE;
    attempting = true;
    attemptStartedMs = millis();
    connectedSinceMs = 0;
    previouslyOnline = false;
    lastReason = reason;
    Serial.printf("WiFi: trying %s profile (SSID length=%u), reason=%s\n", profileName(profile),
                  static_cast<unsigned>(ssid(profile).length()), reason);
}

void scheduleRetry(const char *reason)
{
    WiFi.disconnect(false, false);
    attempting = false;
    targetProfile = WifiProfile::NONE;
    activeProfile = WifiProfile::NONE;
    nextRetryMs = millis() + RETRY_INTERVAL_MS;
    lastReason = reason;
    startFallbackAp();
}

void tryPreferred()
{
    if (configured(WifiProfile::HOME)) beginProfile(WifiProfile::HOME, "preferred retry");
    else if (configured(WifiProfile::MOBILE)) beginProfile(WifiProfile::MOBILE, "mobile-only retry");
    else scheduleRetry("no WiFi configured");
}

void pollHomeScan(uint32_t now)
{
    if (activeProfile != WifiProfile::MOBILE || !configured(WifiProfile::HOME)) return;
    if (!scanRunning && now - lastHomeScanMs >= HOME_SCAN_INTERVAL_MS) {
        lastHomeScanMs = now;
        if (WiFi.scanNetworks(true, true) == WIFI_SCAN_RUNNING) {
            scanRunning = true;
            lastReason = "scanning for home";
        } else {
            lastReason = "home scan start failed";
        }
        return;
    }
    if (!scanRunning) return;
    const int count = WiFi.scanComplete();
    if (count == WIFI_SCAN_RUNNING) return;
    scanRunning = false;
    bool homeVisible = false;
    if (count > 0) {
        for (int i = 0; i < count; ++i) {
            if (WiFi.SSID(i) == c6Config.wifiSsid) { homeVisible = true; break; }
        }
    }
    WiFi.scanDelete();
    if (homeVisible) beginProfile(WifiProfile::HOME, "home visible");
    else lastReason = "home not visible";
}
}

void c6NetworkSetup()
{
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(false);
    WiFi.setHostname(motHostname().c_str());
    if (!configured(WifiProfile::HOME) && !configured(WifiProfile::MOBILE)) {
        lastReason = "no WiFi configured";
        startFallbackAp();
        return;
    }
    tryPreferred();
}

void c6NetworkLoop()
{
    const uint32_t now = millis();
    const bool online = stationOnline();
    if (online) {
        if (!previouslyOnline) {
            activeProfile = targetProfile;
            attempting = false;
            connectedSinceMs = now;
            lastReason = String(profileName(activeProfile)) + " connected";
            Serial.printf("WiFi: %s connected ip=%s rssi=%d dBm\n", profileName(activeProfile),
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
            if (!mdnsStarted) mdnsStarted = MDNS.begin(motHostname().c_str());
        }
        previouslyOnline = true;
        if (apActive && now - connectedSinceMs >= STABLE_INTERVAL_MS) stopFallbackAp();
        pollHomeScan(now);
        return;
    }

    if (previouslyOnline) {
        lastReason = String(profileName(activeProfile)) + " lost";
        Serial.printf("WiFi: %s connection lost\n", profileName(activeProfile));
        activeProfile = WifiProfile::NONE;
        attempting = false;
        nextRetryMs = now;
    }
    previouslyOnline = false;

    if (attempting) {
        if (now - attemptStartedMs < CONNECT_TIMEOUT_MS) return;
        if (targetProfile == WifiProfile::HOME && configured(WifiProfile::MOBILE)) {
            beginProfile(WifiProfile::MOBILE, "home timeout");
        } else {
            scheduleRetry(targetProfile == WifiProfile::HOME ? "home timeout" : "mobile timeout");
        }
        return;
    }

    if (static_cast<int32_t>(now - nextRetryMs) >= 0) tryPreferred();
}

bool c6NetworkOnline() { return stationOnline(); }
String c6NetworkIp() { return c6NetworkOnline() ? WiFi.localIP().toString() : String(); }
int c6NetworkRssi() { return c6NetworkOnline() ? WiFi.RSSI() : 0; }
String c6NetworkProfileName() { return profileName(activeProfile); }
String c6NetworkStateName()
{
    if (c6NetworkOnline()) return "connected";
    if (attempting) return "connecting";
    if (apActive) return "fallback-ap";
    return "retry-wait";
}
String c6NetworkReason() { return lastReason; }
bool c6NetworkHomeConfigured() { return configured(WifiProfile::HOME); }
bool c6NetworkMobileConfigured() { return configured(WifiProfile::MOBILE); }

String c6NetworkStatus()
{
    String value = c6NetworkStateName() + " profile=" + c6NetworkProfileName();
    if (c6NetworkOnline()) value += " ip=" + c6NetworkIp() + " rssi=" + String(c6NetworkRssi()) + " dBm";
    if (apActive) value += " ap=" + motFallbackApSsid();
    value += " reason=" + lastReason;
    return value;
}

bool c6NetworkApActive() { return apActive; }
String c6NetworkApSsid() { return motFallbackApSsid(); }
String c6NetworkApPassword() { return activeApPassword(); }
