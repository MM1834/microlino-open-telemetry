#include "abrp_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>

#include "app_config.h"
#include "telemetry/telemetry.h"
#include "gps/wroom_gps.h"

static unsigned long lastAbrpSendMs = 0;
static const uint32_t ABRP_INTERVAL_MS = 60000;
static AbrpStatus status;

static bool validUnixTime(time_t t)
{
    return t > 1700000000; // roughly 2023-11-14
}

bool abrpEnabled()
{
    String key = config.abrpApiKey;
    String token = config.abrpUserToken;
    key.trim();
    token.trim();
    return !key.isEmpty() && !token.isEmpty();
}

AbrpStatus abrpStatus()
{
    status.enabled = abrpEnabled();
    return status;
}

static String jsonEscape(const String& s)
{
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '\\' || c == '"') {
            out += '\\';
            out += c;
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else {
            out += c;
        }
    }
    return out;
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

static String abrpTelemetryJson(time_t now)
{
    String json = "{";
    bool first = true;

    auto addNumber = [&](const char *key, double value, int decimals) {
        if (!isfinite(value)) return;
        if (!first) json += ",";
        first = false;
        json += "\"";
        json += key;
        json += "\":";
        json += String(value, decimals);
    };

    auto addBool = [&](const char *key, bool value) {
        if (!first) json += ",";
        first = false;
        json += "\"";
        json += key;
        json += "\":";
        json += value ? "true" : "false";
    };

    auto addUInt = [&](const char *key, uint32_t value) {
        if (!first) json += ",";
        first = false;
        json += "\"";
        json += key;
        json += "\":";
        json += String(value);
    };

    // Same fields as the working Node-RED flow:
    // { soc, utc, speed, power, is_charging }
    addNumber("soc", telemetry.display.soc, 1);
    addUInt("utc", (uint32_t)now);
    addNumber("speed", telemetry.display.speedKmh, 1);

    // Current MOT power decoding is still experimental.
    // Keep it signed and pass it as ABRP `power` in kW-equivalent units used by the existing decoder.
    addNumber("power", telemetry.charging.powerSigned, 2);

    addBool("is_charging", telemetry.charging.isCharging);

    // ABRP must never receive a cached or stale position.
    if (wroomGpsValid()) {
        addNumber("lat", wroomGpsLatitude(), 6);
        addNumber("lon", wroomGpsLongitude(), 6);
    }

    json += "}";
    return json;
}

String abrpStatusJson()
{
    AbrpStatus s = abrpStatus();
    const time_t now = time(nullptr);

    String json = "{";
    json += "\"enabled\":" + String(s.enabled ? "true" : "false") + ",";
    json += "\"timeValid\":" + String(validUnixTime(now) ? "true" : "false") + ",";
    json += "\"utc\":" + String((uint32_t)now) + ",";
    json += "\"lastSuccess\":" + String(s.lastSuccess ? "true" : "false") + ",";
    json += "\"lastHttpCode\":" + String(s.lastHttpCode) + ",";
    json += "\"lastMessage\":\"" + jsonEscape(s.lastMessage) + "\",";
    json += "\"lastSendMs\":" + String(s.lastSendMs) + ",";
    json += "\"lastAttemptMs\":" + String(s.lastAttemptMs) + ",";
    json += "\"lastPayload\":\"" + jsonEscape(s.lastPayload) + "\"";
    json += "}";
    return json;
}

bool sendAbrpTelemetryNow()
{
    status.enabled = abrpEnabled();
    status.lastAttemptMs = millis();

    if (!status.enabled) {
        status.lastSuccess = false;
        status.lastHttpCode = 0;
        status.lastMessage = "ABRP disabled: API key or user token missing";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        status.lastSuccess = false;
        status.lastHttpCode = 0;
        status.lastMessage = "WiFi not connected";
        return false;
    }

    const time_t now = time(nullptr);
    if (!validUnixTime(now)) {
        status.lastSuccess = false;
        status.lastHttpCode = 0;
        status.lastMessage = "ABRP waiting for valid system time (NTP)";
        status.lastPayload = "";
        Serial.println("ABRP: waiting for valid system time (NTP)");
        return false;
    }

    String tlm = abrpTelemetryJson(now);
    status.lastPayload = tlm;

    if (tlm == "{}") {
        status.lastSuccess = false;
        status.lastHttpCode = 0;
        status.lastMessage = "No telemetry values available";
        return false;
    }

    String url = "https://api.iternio.com/1/tlm/send";
    url += "?api_key=" + urlEncode(config.abrpApiKey);
    url += "&token=" + urlEncode(config.abrpUserToken);
    url += "&tlm=" + urlEncode(tlm);

    HTTPClient http;
    http.setTimeout(7000);

    if (!http.begin(url)) {
        status.lastSuccess = false;
        status.lastHttpCode = 0;
        status.lastMessage = "http.begin failed";
        return false;
    }

    const int code = http.GET();
    status.lastHttpCode = code;

    if (code > 0) {
        const String body = http.getString();
        status.lastSuccess = (code >= 200 && code < 300);
        status.lastMessage = body.length() ? body : ("HTTP " + String(code));
        if (status.lastSuccess) {
            status.lastSendMs = millis();
        }
        Serial.printf("ABRP: HTTP %d %s\n", code, status.lastSuccess ? "OK" : "FAIL");
    } else {
        status.lastSuccess = false;
        status.lastMessage = http.errorToString(code);
        Serial.printf("ABRP: send failed, %s\n", status.lastMessage.c_str());
    }

    http.end();
    return status.lastSuccess;
}

void setupAbrp()
{
    status.enabled = abrpEnabled();
    Serial.println(status.enabled ? "ABRP: enabled" : "ABRP: disabled (missing API key or user token)");
}

void abrpLoop()
{
    if (!abrpEnabled()) return;

    const unsigned long now = millis();
    if (now - lastAbrpSendMs < ABRP_INTERVAL_MS) return;

    lastAbrpSendMs = now;
    sendAbrpTelemetryNow();
}
