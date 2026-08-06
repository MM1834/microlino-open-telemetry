#include "c6_network.h"

#include <WiFi.h>
#include <time.h>

#include "c6_config.h"
#include "system/device_id.h"

namespace {
uint32_t lastReconnectMs = 0;
}

void c6NetworkSetup()
{
    if (c6Config.wifiSsid.isEmpty()) {
        Serial.println("WiFi: not configured (use wifi set <ssid>|<password>)");
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname(motHostname().c_str());
    WiFi.begin(c6Config.wifiSsid.c_str(), c6Config.wifiPassword.c_str());
    Serial.printf("WiFi: connecting to configured SSID (length=%u)\n",
                  static_cast<unsigned>(c6Config.wifiSsid.length()));
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
}

void c6NetworkLoop()
{
    if (c6Config.wifiSsid.isEmpty() || WiFi.status() == WL_CONNECTED) return;
    if (millis() - lastReconnectMs < 15000) return;
    lastReconnectMs = millis();
    WiFi.reconnect();
}

bool c6NetworkOnline() { return WiFi.status() == WL_CONNECTED; }
String c6NetworkIp() { return c6NetworkOnline() ? WiFi.localIP().toString() : String(); }
int c6NetworkRssi() { return c6NetworkOnline() ? WiFi.RSSI() : 0; }

String c6NetworkStatus()
{
    if (c6Config.wifiSsid.isEmpty()) return "not configured";
    if (!c6NetworkOnline()) return "connecting/disconnected";
    return "connected ip=" + c6NetworkIp() + " rssi=" + String(c6NetworkRssi()) + " dBm";
}
