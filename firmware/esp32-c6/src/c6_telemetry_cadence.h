#pragma once

#include <Arduino.h>

void c6TelemetryCadenceSetup();
void c6TelemetryCadenceUpdate();
uint32_t c6TelemetryCadenceEventGeneration();
uint32_t c6TelemetryCadenceIntervalMs(uint32_t activeIntervalMs);
const char *c6TelemetryCadenceMode();
