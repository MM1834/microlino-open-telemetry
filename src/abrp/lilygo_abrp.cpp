#include "lilygo_abrp.h"

#include <Arduino.h>
#include <Client.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

#include "config/lilygo_config.h"
#include "gps/l76k_gps.h"
#include "modem/lilygo_modem.h"
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

// WiFi can use HTTPS through HTTPClient.
// LTE uses the LewisXhe TinyGSM Client exposed by the modem stack. That client
// is plain TCP in MOT, therefore the LTE fallback uses plain HTTP on port 80.
static const char* ABRP_WIFI_URL = "https://api.iternio.com/1/tlm/send";
static const char* ABRP_LTE_URL  = "http://api.iternio.com/1/tlm/send";

struct AbrpHttpResult
{
    int code = 0;
    String body;
    String error;
};

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

static bool lteAvailable()
{
    return lilygoGprsConnected() || lilygoEnsureGprsConnected();
}

static String currentTransport()
{
    if (WiFi.status() == WL_CONNECTED) return "WiFi";
    if (lilygoGprsConnected()) return "LTE";
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

static bool parseHttpUrl(const String& url, String& host, uint16_t& port, String& path)
{
    String u = url;

    if (u.startsWith("http://")) {
        u.remove(0, 7);
        port = 80;
    } else {
        return false;
    }

    int slash = u.indexOf('/');
    if (slash < 0) {
        host = u;
        path = "/";
    } else {
        host = u.substring(0, slash);
        path = u.substring(slash);
    }

    int colon = host.indexOf(':');
    if (colon >= 0) {
        port = (uint16_t)host.substring(colon + 1).toInt();
        host = host.substring(0, colon);
    }

    host.trim();
    return host.length() > 0 && port > 0;
}

static AbrpHttpResult httpGetViaLte(const String& url)
{
    AbrpHttpResult result;

    String host;
    String path;
    uint16_t port = 80;

    if (!parseHttpUrl(url, host, port, path)) {
        result.error = "LTE ABRP supports only plain http:// URLs";
        return result;
    }

    if (!lteAvailable()) {
        result.error = "LTE/GPRS unavailable";
        return result;
    }

    Client* client = lilygoTinyGsmClient();
    if (!client) {
        result.error = "No LTE client";
        return result;
    }

    client->stop();

    if (!client->connect(host.c_str(), port)) {
        result.error = "LTE TCP connect failed";
        client->stop();
        return result;
    }

    client->print(String("GET ") + path + " HTTP/1.1\r\n");
    client->print(String("Host: ") + host + "\r\n");
    client->print("User-Agent: MOT-LilyGO\r\n");
    client->print("Accept: application/json,*/*\r\n");
    client->print("Connection: close\r\n\r\n");

    String raw;
    bool sawData = false;
    uint32_t start = millis();

    while (millis() - start < 20000) {
        while (client->available()) {
            sawData = true;
            raw += (char)client->read();

            if (raw.length() > 4096) {
                raw.remove(0, raw.length() - 4096);
            }
        }

        if (sawData && !client->connected()) {
            break;
        }

        delay(10);
    }

    client->stop();

    if (raw.startsWith("HTTP/")) {
        int p = raw.indexOf(' ');
        if (p >= 0) {
            result.code = raw.substring(p + 1, p + 4).toInt();
        }
    }

    int bodyStart = raw.indexOf("\r\n\r\n");
    result.body = bodyStart >= 0 ? raw.substring(bodyStart + 4) : raw;

    if (result.code == 0) {
        result.error = raw.length() ? ("HTTP parse failed: " + raw) : "No HTTP response";
    }

    return result;
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
        Serial.println("ABRP: enabled");
        lastMessage = "ABRP enabled";
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

    bool wifi = WiFi.status() == WL_CONNECTED;
    bool lte = !wifi && lteAvailable();

    if (!wifi && !lte) {
        lastSuccess = false;
        lastHttpCode = 0;
        lastTransport = "";
        lastMessage = "ABRP transport unavailable: no WiFi/LTE route";
        failCount++;
        return false;
    }

    // NTP best-effort. With LTE-only operation we keep existing time if valid;
    // ABRP can also accept telemetry without utc.
    if (wifi && !timeValid()) {
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

    String baseUrl = wifi ? String(ABRP_WIFI_URL) : String(ABRP_LTE_URL);
    String url = baseUrl +
        "?api_key=" + config.abrpApiKey +
        "&token=" + config.abrpUserToken +
        "&tlm=" + urlEncode(lastPayload);

    String response;

    if (wifi) {
        lastTransport = "WiFi";

        HTTPClient http;
        http.begin(url);
        lastHttpCode = http.GET();
        response = http.getString();
        http.end();
    } else {
        lastTransport = "LTE";

        AbrpHttpResult r = httpGetViaLte(url);
        lastHttpCode = r.code;
        response = r.body.length() ? r.body : r.error;
    }

    lastSuccess = lastHttpCode >= 200 && lastHttpCode < 300;
    lastMessage = response.length() ? response : (lastSuccess ? "OK" : "HTTP failed");

    if (lastSuccess) {
        successCount++;
        lastSendMs = millis();
        Serial.printf("ABRP %s: HTTP %d OK\n", lastTransport.c_str(), lastHttpCode);
    } else {
        failCount++;
        Serial.printf("ABRP %s: HTTP %d failed: %s\n", lastTransport.c_str(), lastHttpCode, lastMessage.c_str());
    }

    return lastSuccess;
}

void lilygoAbrpLoop()
{
    if (!abrpEnabled()) return;

    if (millis() - lastAttemptMs >= ABRP_INTERVAL_MS) {
        sendLilygoAbrpTelemetryNow();
    }
}

String lilygoAbrpStatusJson()
{
    String transport = currentTransport();
    bool transportAvailable = transport.length() > 0;

    String json = "{";
    json += "\"enabled\":" + String(abrpEnabled() ? "true" : "false") + ",";
    json += "\"configured\":" + String(config.abrpApiKey.length() && config.abrpUserToken.length() ? "true" : "false") + ",";
    json += "\"transport\":\"" + transport + "\",";
    json += "\"lastTransport\":\"" + lastTransport + "\",";
    json += "\"transportAvailable\":" + String(transportAvailable ? "true" : "false") + ",";
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
