#pragma once
#include <Arduino.h>

struct AbrpStatus {
    bool enabled = false;
    bool lastSuccess = false;
    int lastHttpCode = 0;
    String lastMessage;
    uint32_t lastSendMs = 0;
    uint32_t lastAttemptMs = 0;
    String lastPayload;
};

void setupAbrp();
void abrpLoop();
bool abrpEnabled();
bool sendAbrpTelemetryNow();
String abrpStatusJson();
AbrpStatus abrpStatus();
