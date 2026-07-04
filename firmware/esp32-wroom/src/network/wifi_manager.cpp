#include "wifi_manager.h"
#include "../app_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include "system/device_id.h"
#include "telemetry/telemetry.h"

static NetworkState currentState = NetworkState::FALLBACK_AP;

static void startFallbackAp()
{
    String ssid = motFallbackApSsid();
    Serial.printf("Starting fallback AP: %s\n", ssid.c_str());

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str());

    currentState = NetworkState::FALLBACK_AP;
    telemetry.system.networkMode = networkModeName();
    telemetry.system.ipAddress = WiFi.softAPIP().toString();

    Serial.printf("Fallback AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void setupNetwork()
{
    if (config.wifiSsid.isEmpty()) {
        Serial.println("No WiFi configured.");
        startFallbackAp();
        return;
    }

    Serial.printf("Connecting WiFi '%s'\n", config.wifiSsid.c_str());

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(300);

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname(motHostname().c_str());

    WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 40000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        currentState = NetworkState::STA;
        telemetry.system.networkMode = networkModeName();
        telemetry.system.ipAddress = WiFi.localIP().toString();
        telemetry.system.wifiRssi = WiFi.RSSI();

        Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());

       	configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
        Serial.println("NTP: time sync requested");


        if (MDNS.begin(motHostname().c_str())) {
            Serial.printf("mDNS: http://%s.local/\n", motHostname().c_str());
        }
        return;
    }

    Serial.printf("\nWiFi failed, status=%d. Starting fallback AP.\n", WiFi.status());
    startFallbackAp();
}

bool networkOnline()
{
    return currentState == NetworkState::STA && WiFi.status() == WL_CONNECTED;
}

String networkIp()
{
    return currentState == NetworkState::STA ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

String networkModeName()
{
    switch (currentState) {
        case NetworkState::STA: return "WiFi STA";
        case NetworkState::FALLBACK_AP: return "Fallback AP";
        case NetworkState::LTE: return "LTE";
        default: return "Unknown";
    }
}

int networkRssi()
{
    return networkOnline() ? WiFi.RSSI() : 0;
}

NetworkState networkState()
{
    return currentState;
}
