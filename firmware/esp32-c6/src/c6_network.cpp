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
constexpr uint32_t WEAK_LINK_GRACE_MS = 20000;
constexpr int HOME_WEAK_RSSI_DBM = -88;
constexpr int HOME_RECOVER_RSSI_DBM = -80;

WifiProfile targetProfile = WifiProfile::NONE;
WifiProfile activeProfile = WifiProfile::NONE;
uint32_t attemptStartedMs = 0;
uint32_t connectedSinceMs = 0;
uint32_t nextRetryMs = 0;
uint32_t lastHomeScanMs = 0;
uint32_t weakLinkSinceMs = 0;
uint32_t transitionCount = 0;
uint32_t lastTransitionMs = 0;
volatile uint32_t disconnectCount = 0;
volatile uint32_t lastDisconnectMs = 0;
volatile uint8_t lastDisconnectReason = 0;
volatile bool lastDisconnectWasManagerInitiated = false;
volatile bool managerDisconnectPending = false;
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
    managerDisconnectPending = WiFi.status() == WL_CONNECTED;
    WiFi.disconnect(false, false);
    WiFi.begin(ssid(profile).c_str(), password(profile).c_str());
    targetProfile = profile;
    activeProfile = WifiProfile::NONE;
    attempting = true;
    attemptStartedMs = millis();
    connectedSinceMs = 0;
    weakLinkSinceMs = 0;
    previouslyOnline = false;
    transitionCount++;
    lastTransitionMs = attemptStartedMs;
    lastReason = reason;
    Serial.printf("WiFi: trying %s profile (SSID length=%u), reason=%s\n", profileName(profile),
                  static_cast<unsigned>(ssid(profile).length()), reason);
}

void scheduleRetry(const char *reason)
{
    managerDisconnectPending = WiFi.status() == WL_CONNECTED;
    WiFi.disconnect(false, false);
    attempting = false;
    targetProfile = WifiProfile::NONE;
    activeProfile = WifiProfile::NONE;
    weakLinkSinceMs = 0;
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
    int homeRssi = -127;
    if (count > 0) {
        for (int i = 0; i < count; ++i) {
            if (WiFi.SSID(i) == c6Config.wifiSsid) {
                homeVisible = true;
                homeRssi = max(homeRssi, static_cast<int>(WiFi.RSSI(i)));
            }
        }
    }
    WiFi.scanDelete();
    if (homeVisible) {
        // Home is preferred independent of Mobile's apparent link quality. A
        // hotspot can retain WiFi and DHCP while its cellular uplink is down.
        // The bounded Home timeout still returns to Mobile if association fails.
        const String reason = "home visible (best " + String(homeRssi) + " dBm)";
        beginProfile(WifiProfile::HOME, reason.c_str());
    }
    else lastReason = "home not visible";
}
}

void c6NetworkSetup()
{
    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
        disconnectCount = disconnectCount + 1;
        lastDisconnectMs = millis();
        lastDisconnectReason = info.wifi_sta_disconnected.reason;
        lastDisconnectWasManagerInitiated = managerDisconnectPending;
        managerDisconnectPending = false;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
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
        const int rssi = WiFi.RSSI();
        const bool homeProfile = activeProfile == WifiProfile::HOME;
        if (homeProfile && !weakLinkSinceMs && rssi <= HOME_WEAK_RSSI_DBM) {
            weakLinkSinceMs = now;
            lastReason = "home link weak (" + String(rssi) + " dBm)";
        }
        if (homeProfile && weakLinkSinceMs) {
            if (rssi >= HOME_RECOVER_RSSI_DBM) {
                weakLinkSinceMs = 0;
                lastReason = "home link recovered";
            }
            else if (now - weakLinkSinceMs >= WEAK_LINK_GRACE_MS) {
                // RSSI alone does not mean that the link is unusable. Keep a
                // working Home association and fall back to Mobile only after
                // the station actually disconnects or cannot obtain an IP.
                if (!lastReason.startsWith("home connected but weak")) {
                    lastReason = "home connected but weak (" + String(rssi) + " dBm)";
                }
            }
        }
        if (apActive && now - connectedSinceMs >= STABLE_INTERVAL_MS) stopFallbackAp();
        pollHomeScan(now);
        return;
    }

    if (previouslyOnline) {
        lastReason = String(profileName(activeProfile)) + " lost";
        Serial.printf("WiFi: %s connection lost\n", profileName(activeProfile));
        activeProfile = WifiProfile::NONE;
        weakLinkSinceMs = 0;
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
bool c6NetworkLinkWeak()
{
    return stationOnline() && activeProfile == WifiProfile::HOME && weakLinkSinceMs != 0;
}
bool c6NetworkTransportReady() { return c6NetworkOnline(); }
uint32_t c6NetworkWeakForMs()
{
    return weakLinkSinceMs ? millis() - weakLinkSinceMs : 0;
}
uint32_t c6NetworkTransitionCount() { return transitionCount; }
uint32_t c6NetworkLastTransitionAgeMs()
{
    return lastTransitionMs ? millis() - lastTransitionMs : 0;
}
String c6NetworkBssid() { return c6NetworkOnline() ? WiFi.BSSIDstr() : String(); }
int c6NetworkChannel() { return c6NetworkOnline() ? WiFi.channel() : 0; }
uint32_t c6NetworkDisconnectCount() { return disconnectCount; }
uint8_t c6NetworkLastDisconnectReason() { return lastDisconnectReason; }
String c6NetworkLastDisconnectReasonName()
{
    return lastDisconnectReason
        ? String(WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(lastDisconnectReason)))
        : String("none");
}
uint32_t c6NetworkLastDisconnectAgeMs()
{
    return lastDisconnectMs ? millis() - lastDisconnectMs : 0;
}
bool c6NetworkLastDisconnectWasManagerInitiated()
{
    return lastDisconnectWasManagerInitiated;
}
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
    if (c6NetworkLinkWeak()) value += " weakForMs=" + String(c6NetworkWeakForMs());
    if (apActive) value += " ap=" + motFallbackApSsid();
    value += " reason=" + lastReason;
    return value;
}

bool c6NetworkApActive() { return apActive; }
String c6NetworkApSsid() { return motFallbackApSsid(); }
String c6NetworkApPassword() { return activeApPassword(); }
