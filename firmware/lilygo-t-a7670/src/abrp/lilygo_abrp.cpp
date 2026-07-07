#include "lilygo_abrp.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

#include "config/lilygo_config.h"
#include "gps/l76k_gps.h"
#include "network/lilygo_network.h"
#include "telemetry/telemetry.h"

static bool lastSuccess = false;
static int lastHttpCode = 0;
static String lastMessage = "";
static String lastPayload = "";
static String lastTransport = "";
static unsigned long lastSendMs = 0;
static unsigned long lastAttemptMs = 0;
static uint32_t successCount = 0;
static uint32_t failCount = 0;

static const unsigned long ABRP_INTERVAL_MS = 30000;
static const char* ABRP_URL = "https://api.iternio.com/1/tlm/send";

static String esc(String s)
{
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    s.replace("\r", "\\r");
    s.replace("\n", "\\n");
    return s;
}

static bool abrpEnabled()
{
    return config.abrpEnabled && config.abrpApiKey.length() && config.abrpUserToken.length();
}

static bool timeValid()
{
    time_t now = time(nullptr);
    return now > 1700000000;
}

static String currentTransport()
{
    if (WiFi.status() == WL_CONNECTED) return "WiFi";
    if (lilygoNetworkModeName() == "LTE") return "LTE";
    return "";
}

static String urlEncode(const String& s)
{
    String out;
    const char *hex = "0123456789ABCDEF";

    for (size_t i = 0; i < s.length(); i++) {
        uint8_t c = (uint8_t)s[i];

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }

    return out;
}

static String buildPayload()
{
    String json = "{";
    bool first = true;

    auto addComma = [&]() {
        if (!first) json += ",";
        first = false;
    };

    if (!isnan(telemetry.display.soc)) {
        addComma();
        json += "\"soc\":" + String(telemetry.display.soc, 1);
    }

    if (timeValid()) {
        addComma();
        json += "\"utc\":" + String((uint32_t)time(nullptr));
    }

    if (!isnan(telemetry.display.speedKmh)) {
        addComma();
        json += "\"speed\":" + String(telemetry.display.speedKmh, 1);
    }

    addComma();
    json += "\"power\":" + String(telemetry.charging.powerSigned);

    addComma();
    json += "\"is_charging\":" + String(telemetry.charging.isCharging ? "true" : "false");

    if (l76kGpsValid()) {
        addComma();
        json += "\"lat\":" + String(l76kLatitude(), 6);

        addComma();
        json += "\"lon\":" + String(l76kLongitude(), 6);
    }

    json += "}";
    return json;
}

void setupLilygoAbrp()
{
    if (abrpEnabled()) {
        Serial.println("ABRP: enabled (WiFi transport only in this stability release)");
        lastMessage = "ABRP enabled; LTE HTTPS disabled for stability release";
    } else {
        Serial.println("ABRP: disabled (missing api key/token or disabled)");
        lastMessage = "ABRP disabled";
    }
}

bool sendLilygoAbrpTelemetryNow()
{
    lastAttemptMs = millis();
    lastTransport = currentTransport();

    if (!abrpEnabled()) {
        lastSuccess = false;
        lastHttpCode = 0;
        lastMessage = "ABRP disabled";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        lastSuccess = false;
        lastHttpCode = 0;
        lastMessage = "ABRP LTE HTTPS disabled in stability release; WiFi required";
        failCount++;
        return false;
    }

    lastTransport = "WiFi";

    if (!timeValid()) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    }

    lastPayload = buildPayload();

    if (lastPayload == "{}") {
        lastSuccess = false;
        lastHttpCode = 0;
        lastMessage = "ABRP no telemetry payload yet";
        failCount++;
        return false;
    }

    String url = String(ABRP_URL) +
        "?api_key=" + config.abrpApiKey +
        "&token=" + config.abrpUserToken +
        "&tlm=" + urlEncode(lastPayload);

    HTTPClient http;
    http.begin(url);
    lastHttpCode = http.GET();
    String response = http.getString();
    http.end();

    lastSuccess = lastHttpCode >= 200 && lastHttpCode < 300;
    lastMessage = response.length() ? response : (lastSuccess ? "OK" : "HTTP failed");

    if (lastSuccess) {
        successCount++;
        lastSendMs = millis();
        Serial.printf("ABRP WiFi: HTTP %d OK\n", lastHttpCode);
    } else {
        failCount++;
        Serial.printf("ABRP WiFi: HTTP %d failed: %s\n", lastHttpCode, lastMessage.c_str());
    }

    return lastSuccess;
}

void lilygoAbrpLoop()
{
    if (!abrpEnabled()) return;

    if (WiFi.status() != WL_CONNECTED) return;

    if (millis() - lastAttemptMs >= ABRP_INTERVAL_MS) {
        sendLilygoAbrpTelemetryNow();
    }
}

String lilygoAbrpStatusJson()
{
    String transport = currentTransport();
    bool wifiAvailable = WiFi.status() == WL_CONNECTED;

    String json = "{";
    json += "\"enabled\":" + String(abrpEnabled() ? "true" : "false") + ",";
    json += "\"configured\":" + String(config.abrpApiKey.length() && config.abrpUserToken.length() ? "true" : "false") + ",";
    json += "\"transport\":\"" + transport + "\",";
    json += "\"lastTransport\":\"" + lastTransport + "\",";
    json += "\"transportAvailable\":" + String(wifiAvailable ? "true" : "false") + ",";
    json += "\"lteHttpsEnabled\":false,";
    json += "\"timeValid\":" + String(timeValid() ? "true" : "false") + ",";
    json += "\"lastSuccess\":" + String(lastSuccess ? "true" : "false") + ",";
    json += "\"http\":" + String(lastHttpCode) + ",";
    json += "\"successCount\":" + String(successCount) + ",";
    json += "\"failCount\":" + String(failCount) + ",";
    json += "\"lastAttemptMs\":" + String(lastAttemptMs) + ",";
    json += "\"lastSendMs\":" + String(lastSendMs) + ",";
    json += "\"message\":\"" + esc(lastMessage) + "\",";
    json += "\"payload\":\"" + esc(lastPayload) + "\"";
    json += "}";

    return json;
}
