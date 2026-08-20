#pragma once

#include <Arduino.h>

struct AbrpSettings {
    bool enabled = false;
    String apiKey;
    String userToken;
    uint32_t intervalMs = 60000;
};

struct AbrpLocation {
    bool valid = false;
    double latitude = 0.0;
    double longitude = 0.0;
};

using AbrpLocationProvider = bool (*)(AbrpLocation &location);

struct AbrpStatus {
    bool enabled = false;
    bool inFlight = false;
    bool lastSuccess = false;
    int lastHttpCode = 0;
    String lastMessage;
    uint32_t lastSendMs = 0;
    uint32_t lastAttemptMs = 0;
    uint32_t heapBefore = 0;
    uint32_t heapAfter = 0;
    uint32_t largestFreeBlock = 0;
    uint32_t lowMemorySkips = 0;
    String lastPayload;
};

void setupAbrp(const AbrpSettings &settings);
void abrpLoop(const AbrpSettings &settings, AbrpLocationProvider locationProvider = nullptr);
bool abrpEnabled(const AbrpSettings &settings);
bool queueAbrpTelemetry(const AbrpSettings &settings, AbrpLocationProvider locationProvider = nullptr);
String abrpStatusJson(const AbrpSettings &settings);
AbrpStatus abrpStatus(const AbrpSettings &settings);
