#pragma once

#include <Arduino.h>

void c6AwsSetup();
void c6AwsLoop();
void c6AwsPrepareNetwork();
void c6AwsDisconnect();
String c6AwsStatus();
bool c6AwsConnected();
bool c6AwsRecentlyHealthy();
bool c6AwsAllowsAbrp();
bool c6AwsReadyForTlsPause();
void c6AwsVolumeCounters(
    uint32_t &publishCount, uint64_t &payloadBytes, uint64_t &mqttPublishBytes,
    uint32_t &connectAttempts, uint32_t &reconnects, uint32_t &disconnects);
