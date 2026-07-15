#include "lilygo_network.h"

#include <WiFi.h>

#include "board_config.h"
#include "config/lilygo_config.h"
#include "modem/lilygo_modem.h"

static LilygoNetworkMode mode = LilygoNetworkMode::AP_ONLY;
static String apSsid;

static unsigned long lastCheckMs = 0;
static unsigned long wifiLostSinceMs = 0;
static unsigned long lastWifiRetryMs = 0;
static unsigned long lastLteRetryMs = 0;

static const unsigned long WIFI_LOSS_GRACE_MS = 20000;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 60000;
static const unsigned long LTE_RETRY_INTERVAL_MS = 15000;

static String lastMessage = "";

static String chipId()
{
    uint64_t mac = ESP.getEfuseMac();
    char buf[16];
    snprintf(buf, sizeof(buf), "%06X", (uint32_t)(mac & 0xFFFFFF));
    return String(buf);
}

static bool wifiConfigured()
{
    String ssid = config.wifiSsid;
    ssid.trim();
    return !ssid.isEmpty();
}

static void startAp()
{
    apSsid = String(SETUP_AP_SSID) + "-" + chipId();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid.c_str());
    // WiFi.softAP(apSsid.c_str(), SETUP_AP_PASS);

    Serial.printf("Setup AP: %s\n", apSsid.c_str());
    Serial.printf("Setup AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

static bool tryWifi(uint32_t timeoutMs)
{
    if (!wifiConfigured()) {
        lastMessage = "WiFi not configured";
        return false;
    }

    Serial.printf(
        "WiFi STA: SSID='%s' length=%u\n",
        config.wifiSsid.c_str(),
        static_cast<unsigned>(config.wifiSsid.length())
    );

    Serial.printf(
        "WiFi STA: password length=%u\n",
        static_cast<unsigned>(config.wifiPass.length())
    );


    Serial.printf("WiFi STA: connecting to '%s'\n", config.wifiSsid.c_str());
    lastMessage = "WiFi connect attempt";

    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        mode = LilygoNetworkMode::WIFI_CLIENT;
        wifiLostSinceMs = 0;
        lastMessage = "WiFi connected";
        Serial.printf("Network: WiFi %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    lastMessage = "WiFi connect failed";
    Serial.printf("WiFi STA failed status=%d\n", WiFi.status());
    return false;
}

static bool tryLte()
{
    if (lilygoGprsConnected()) {
        mode = LilygoNetworkMode::LTE;
        lastMessage = "LTE connected";

        Serial.printf(
        "Network: LTE connected ip=%s\n",
        lilygoLteIp().c_str()
    );
        return true;
    }

    if (millis() - lastLteRetryMs < LTE_RETRY_INTERVAL_MS) return false;
    lastLteRetryMs = millis();

    Serial.println("Network: LTE ensure/retry");
    lastMessage = "LTE retry";
    if (lilygoEnsureGprsConnected()) {
        mode = LilygoNetworkMode::LTE;
        lastMessage = "LTE connected";
        Serial.println("Network: LTE connected");
        return true;
    }

    lastMessage = "LTE not ready";
    return false;
}

void setupLilygoNetwork()
{
    startAp();

    if (tryWifi(20000)) return;

    if (tryLte()) return;

    mode = LilygoNetworkMode::AP_ONLY;
    lastMessage = "AP only";
    Serial.println("Network: AP only");
}

void lilygoNetworkLoop()
{
    const unsigned long now = millis();
    if (now - lastCheckMs < 5000) return;
    lastCheckMs = now;

    const bool wifiConnected = WiFi.status() == WL_CONNECTED;


    if (wifiConnected) {
        if (mode != LilygoNetworkMode::WIFI_CLIENT) {
              Serial.printf("Network: switched to WiFi ip=%s rssi=%d\n",
              WiFi.localIP().toString().c_str(),
              WiFi.RSSI());
        }
        mode = LilygoNetworkMode::WIFI_CLIENT;
        wifiLostSinceMs = 0;
        lastMessage = "WiFi connected";
        return;
    }

    if (mode == LilygoNetworkMode::WIFI_CLIENT && !wifiConnected) {
        if (wifiLostSinceMs == 0) {
            wifiLostSinceMs = now;
            lastMessage = "WiFi lost, waiting grace period";
            Serial.println("Network: WiFi lost, waiting before LTE failover");
            return;
        }

        if (now - wifiLostSinceMs < WIFI_LOSS_GRACE_MS) {
            lastMessage = "WiFi lost, grace period";
            return;
        }

        Serial.println("Network: WiFi lost, failing over to LTE");
    }

    if (tryLte()) {
        // While on LTE, occasionally try to return to WiFi if configured.
        if (wifiConfigured() && now - lastWifiRetryMs > WIFI_RETRY_INTERVAL_MS) {
            lastWifiRetryMs = now;
            tryWifi(8000);
        }
        return;
    }

    // LTE not ready. Retry WiFi periodically if configured.
    if (wifiConfigured() && now - lastWifiRetryMs > WIFI_RETRY_INTERVAL_MS) {
        lastWifiRetryMs = now;
        if (tryWifi(8000)) return;
    }

    mode = LilygoNetworkMode::AP_ONLY;
}

bool lilygoNetworkOnline()
{
    return mode == LilygoNetworkMode::WIFI_CLIENT || mode == LilygoNetworkMode::LTE;
}

String lilygoNetworkModeName()
{
    if (mode == LilygoNetworkMode::WIFI_CLIENT) return "WiFi";
    if (mode == LilygoNetworkMode::LTE) return "LTE";
    return "AP only";
}

String lilygoNetworkIp()
{
    if (mode == LilygoNetworkMode::WIFI_CLIENT) {
        return WiFi.localIP().toString();
    }
    if (mode == LilygoNetworkMode::LTE) {
        return lilygoLteIp();
    }
    return WiFi.softAPIP().toString();
}

static String esc(String s)
{
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    s.replace("\r", "\\r");
    s.replace("\n", "\\n");
    return s;
}

String lilygoNetworkStatusJson()
{
    String json = "{";
    json += "\"mode\":\"" + lilygoNetworkModeName() + "\",";
    json += "\"online\":" + String(lilygoNetworkOnline() ? "true" : "false") + ",";
    json += "\"ip\":\"" + lilygoNetworkIp() + "\",";
    json += "\"wifiConfigured\":" + String(wifiConfigured() ? "true" : "false") + ",";
    json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"wifiRssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
    json += "\"lteConnected\":" + String(lilygoGprsConnected() ? "true" : "false") + ",";
    json += "\"apSsid\":\"" + esc(apSsid) + "\",";
    json += "\"wifiLossGraceMs\":" + String(WIFI_LOSS_GRACE_MS) + ",";
    json += "\"wifiRetryIntervalMs\":" + String(WIFI_RETRY_INTERVAL_MS) + ",";
    json += "\"lteRetryIntervalMs\":" + String(LTE_RETRY_INTERVAL_MS) + ",";
    json += "\"message\":\"" + esc(lastMessage) + "\"";
    json += "}";
    return json;
}
