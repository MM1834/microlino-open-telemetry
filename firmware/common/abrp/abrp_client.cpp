#include "abrp_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <time.h>

#include "telemetry/telemetry.h"

namespace {
constexpr char ABRP_URL[] = "https://api.iternio.com/1/tlm/send";
constexpr uint32_t MIN_INTERVAL_MS = 15000;
constexpr uint32_t DEFAULT_INTERVAL_MS = 60000;
constexpr uint32_t HTTP_TIMEOUT_MS = 7000;
constexpr uint32_t MIN_FREE_HEAP_BYTES = 96000;
constexpr uint32_t MIN_LARGEST_BLOCK_BYTES = 64000;

struct AbrpTaskInput {
    String apiKey;
    String userToken;
    String payload;
};

AbrpStatus currentStatus;
SemaphoreHandle_t statusMutex = nullptr;
uint32_t lastQueuedMs = 0;

bool validUnixTime(time_t value) { return value > 1700000000; }

void lockStatus()
{
    if (statusMutex != nullptr) xSemaphoreTake(statusMutex, portMAX_DELAY);
}

void unlockStatus()
{
    if (statusMutex != nullptr) xSemaphoreGive(statusMutex);
}

String jsonEscape(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\n", "\\n");
    value.replace("\r", "\\r");
    return value;
}

String urlEncode(const String &value)
{
    static const char *hex = "0123456789ABCDEF";
    String out;
    out.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(value[i]);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%'; out += hex[c >> 4]; out += hex[c & 0x0F];
        }
    }
    return out;
}

String telemetryJson(time_t now, AbrpLocationProvider locationProvider)
{
    String json = "{";
    bool first = true;
    auto addNumber = [&](const char *key, double value, int decimals) {
        if (!isfinite(value)) return;
        if (!first) json += ','; first = false;
        json += '"'; json += key; json += "\":"; json += String(value, decimals);
    };
    auto addBool = [&](const char *key, bool value) {
        if (!first) json += ','; first = false;
        json += '"'; json += key; json += "\":"; json += value ? "true" : "false";
    };
    addNumber("soc", telemetry.display.soc, 1);
    addNumber("utc", static_cast<double>(now), 0);
    addNumber("speed", telemetry.display.speedKmh, 1);
    const bool freshBmsPower = telemetry.bms.packCurrentValid &&
        millis() - telemetry.bms.packCurrentLastUpdateMs <= 10000;
    // ABRP expects vehicle power in kW: traction positive, charge/regen negative.
    // MOT's legacy display value is stored in 0.1 kW units.
    addNumber("power", freshBmsPower
        ? telemetry.bms.vehiclePowerW / 1000.0
        : telemetry.charging.powerSigned / 10.0, 2);
    // Prefer the verified Standard-CAN decoder whenever both status and current
    // are fresh, matching the AWS publication path. The Display-CAN estimate is
    // only a compatibility fallback for vehicles without Standard-CAN data.
    addBool("is_charging", telemetryIsCharging());

    AbrpLocation location;
    if (locationProvider != nullptr && locationProvider(location) && location.valid) {
        addNumber("lat", location.latitude, 6);
        addNumber("lon", location.longitude, 6);
    }
    json += '}';
    return json;
}

void finishAttempt(bool success, int httpCode, const String &message)
{
    lockStatus();
    currentStatus.inFlight = false;
    currentStatus.lastSuccess = success;
    currentStatus.lastHttpCode = httpCode;
    currentStatus.lastMessage = message;
    if (success) currentStatus.lastSendMs = millis();
    unlockStatus();
}

void performSend(const AbrpTaskInput &input)
{
    String url = ABRP_URL;
    url += "?api_key=" + urlEncode(input.apiKey);
    url += "&token=" + urlEncode(input.userToken);
    url += "&tlm=" + urlEncode(input.payload);

    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(url)) {
        finishAttempt(false, 0, "http.begin failed");
        return;
    }

    const int code = http.GET();
    if (code > 0) {
        const String body = http.getString();
        const bool success = code >= 200 && code < 300;
        finishAttempt(success, code, body.length() ? body : "HTTP " + String(code));
        Serial.printf("ABRP: HTTP %d %s\n", code, success ? "OK" : "FAIL");
    } else {
        const String message = http.errorToString(code);
        finishAttempt(false, code, message);
        Serial.printf("ABRP: send failed, %s\n", message.c_str());
    }
    http.end();
}

void sendTask(void *parameter)
{
    AbrpTaskInput *input = static_cast<AbrpTaskInput *>(parameter);
    // Keep all HTTP/TLS objects in a nested function. It must return before the
    // FreeRTOS task deletes itself, otherwise C++ destructors are skipped and
    // the TLS allocation is leaked on every ABRP transmission.
    performSend(*input);
    delete input;

    lockStatus();
    currentStatus.heapAfter = ESP.getFreeHeap();
    currentStatus.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    unlockStatus();
    vTaskDelete(nullptr);
}
}

bool abrpEnabled(const AbrpSettings &settings)
{
    String key = settings.apiKey;
    String token = settings.userToken;
    key.trim(); token.trim();
    return settings.enabled && !key.isEmpty() && !token.isEmpty();
}

AbrpStatus abrpStatus(const AbrpSettings &settings)
{
    lockStatus();
    AbrpStatus copy = currentStatus;
    unlockStatus();
    copy.enabled = abrpEnabled(settings);
    return copy;
}

String abrpStatusJson(const AbrpSettings &settings)
{
    const AbrpStatus state = abrpStatus(settings);
    const time_t now = time(nullptr);
    String json = "{";
    json += "\"enabled\":" + String(state.enabled ? "true" : "false") + ',';
    json += "\"inFlight\":" + String(state.inFlight ? "true" : "false") + ',';
    json += "\"timeValid\":" + String(validUnixTime(now) ? "true" : "false") + ',';
    json += "\"utc\":" + String(static_cast<uint32_t>(now)) + ',';
    json += "\"lastSuccess\":" + String(state.lastSuccess ? "true" : "false") + ',';
    json += "\"lastHttpCode\":" + String(state.lastHttpCode) + ',';
    json += "\"lastMessage\":\"" + jsonEscape(state.lastMessage) + "\",";
    json += "\"lastSendMs\":" + String(state.lastSendMs) + ',';
    json += "\"lastAttemptMs\":" + String(state.lastAttemptMs) + ',';
    json += "\"heapBefore\":" + String(state.heapBefore) + ',';
    json += "\"heapAfter\":" + String(state.heapAfter) + ',';
    json += "\"largestFreeBlock\":" + String(state.largestFreeBlock) + ',';
    json += "\"lowMemorySkips\":" + String(state.lowMemorySkips) + ',';
    json += "\"lastPayload\":\"" + jsonEscape(state.lastPayload) + "\"}";
    return json;
}

void setupAbrp(const AbrpSettings &settings)
{
    if (statusMutex == nullptr) statusMutex = xSemaphoreCreateMutex();
    lockStatus(); currentStatus.enabled = abrpEnabled(settings); unlockStatus();
    Serial.println(abrpEnabled(settings) ? "ABRP: enabled" : "ABRP: disabled");
}

bool queueAbrpTelemetry(const AbrpSettings &settings, AbrpLocationProvider locationProvider)
{
    const uint32_t nowMs = millis();
    lockStatus();
    const bool busy = currentStatus.inFlight;
    unlockStatus();
    if (busy) return false;

    if (!abrpEnabled(settings)) {
        finishAttempt(false, 0, "ABRP disabled or credentials missing");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        finishAttempt(false, 0, "WiFi not connected");
        return false;
    }
    const time_t now = time(nullptr);
    if (!validUnixTime(now)) {
        finishAttempt(false, 0, "ABRP waiting for valid system time");
        return false;
    }

    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (freeHeap < MIN_FREE_HEAP_BYTES || largestBlock < MIN_LARGEST_BLOCK_BYTES) {
        lockStatus();
        currentStatus.inFlight = false;
        currentStatus.lastSuccess = false;
        currentStatus.lastHttpCode = 0;
        currentStatus.lastMessage = "ABRP deferred: insufficient TLS heap";
        currentStatus.heapBefore = freeHeap;
        currentStatus.heapAfter = freeHeap;
        currentStatus.largestFreeBlock = largestBlock;
        currentStatus.lowMemorySkips++;
        unlockStatus();
        lastQueuedMs = nowMs;
        Serial.printf("ABRP: deferred freeHeap=%u largestBlock=%u\n", freeHeap, largestBlock);
        return false;
    }

    AbrpTaskInput *input = new AbrpTaskInput();
    if (input == nullptr) {
        finishAttempt(false, 0, "Out of memory");
        return false;
    }
    input->apiKey = settings.apiKey;
    input->userToken = settings.userToken;
    input->payload = telemetryJson(now, locationProvider);

    lockStatus();
    currentStatus.inFlight = true;
    currentStatus.lastAttemptMs = nowMs;
    currentStatus.lastPayload = input->payload;
    currentStatus.lastMessage = "Queued";
    currentStatus.heapBefore = freeHeap;
    currentStatus.largestFreeBlock = largestBlock;
    unlockStatus();
    lastQueuedMs = nowMs;

    if (xTaskCreate(sendTask, "mot-abrp", 8192, input, 1, nullptr) != pdPASS) {
        delete input;
        finishAttempt(false, 0, "Could not start ABRP task");
        return false;
    }
    return true;
}

void abrpLoop(const AbrpSettings &settings, AbrpLocationProvider locationProvider)
{
    if (!abrpEnabled(settings)) return;
    const uint32_t interval = settings.intervalMs < MIN_INTERVAL_MS ? DEFAULT_INTERVAL_MS : settings.intervalMs;
    if (millis() - lastQueuedMs < interval) return;
    queueAbrpTelemetry(settings, locationProvider);
}
