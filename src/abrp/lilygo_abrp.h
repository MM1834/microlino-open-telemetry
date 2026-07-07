#pragma once
#include <Arduino.h>

void setupLilygoAbrp();
void lilygoAbrpLoop();

bool sendLilygoAbrpTelemetryNow();
String lilygoAbrpStatusJson();
