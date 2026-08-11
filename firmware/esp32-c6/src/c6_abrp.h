#pragma once

#include <Arduino.h>

void c6AbrpSetup();
void c6AbrpLoop();
bool c6AbrpQueueTelemetry();
String c6AbrpStatusJson();
bool c6AbrpConfigured();
