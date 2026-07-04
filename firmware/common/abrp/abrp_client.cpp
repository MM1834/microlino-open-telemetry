#include "abrp_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "telemetry/telemetry.h"
#include "app_config.h"

static unsigned long lastAbrpSendMs = 0;
static const uint32_t ABRP_INTERVAL_MS = 60000;

bool abrpEnabled()
{
    return !config.abrpApiKey.isEmpty() && !config.abrpUserToken.isEmpty();
}

static String urlEncode(const String& s)
{
    static const char *hex = "0123456789ABCDEF";
    String out;
    out.reserve(s.length() * 3);

    for (size_t i = 0; i < s.length(); i++) {
        const uint8_t c = (uint8_t)s[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }

    return out;
}

static String abrpTelemetryJson()
{
    String json = "{";
    bool first = true;

    auto addNumber = [&](const char *key, float value, int decimals) {
        if (!isfinite(value)) return;
        if (!first) json += ",";
        first = false;
        json += "\""; json += key; json += "\":";
        json += String(value, decimals);
    };

    auto addInt = [&](const char *key, int value) {
        if (value < 0) return;
        if (!first) json += ",";
        first = false;
        json += "\""; json += key; json += "\":";
        json += String(value);
    };

    auto addBool = [&](const char *key, bool value) {
        if (!first) json += ",";
        first = false;
        json += "\""; json += key; json += "\":";
        json += value ? "true" : "false";
    };

    addNumber("soc", telemetry.display.soc, 1);
    addNumber("speed", telemetry.display.speedKmh, 1);
    addInt("est_battery_range", telemetry.display.estimatedRangeKm);
    addBool("is_charging", telemetry.charging.isCharging);

    json += "}";
    return json;
}

static void sendAbrpTelemetry()
{
    if (!abrpEnabled()) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (!telemetry.display.valid && !telemetry.charging.valid) return;

    String tlm = abrpTelemetryJson();
    if (tlm == "{}") return;

    String url = "https://api.iternio.com/1/tlm/send";
    url += "?api_key=" + urlEncode(config.abrpApiKey);
    url += "&token=" + urlEncode(config.abrpUserToken);
    url += "&tlm=" + urlEncode(tlm);

    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(url)) {
        Serial.println("ABRP: http.begin failed");
        return;
    }

    int code = http.GET();
    if (code > 0) {
        Serial.printf("ABRP: telemetry sent, HTTP %d\n", code);
    } else {
        Serial.printf("ABRP: send failed, %s\n", http.errorToString(code).c_str());
    }
    http.end();
}

void setupAbrp()
{
    if (abrpEnabled()) Serial.println("ABRP: enabled");
    else Serial.println("ABRP: disabled (missing API key or user token)");
}

void abrpLoop()
{
    if (!abrpEnabled()) return;
    const unsigned long now = millis();
    if (now - lastAbrpSendMs < ABRP_INTERVAL_MS) return;
    lastAbrpSendMs = now;
    sendAbrpTelemetry();
}
