#pragma once

#include <Arduino.h>

void setupWroomAbrp();
void wroomAbrpLoop();
bool queueWroomAbrpTelemetry();
String wroomAbrpStatusJson();
