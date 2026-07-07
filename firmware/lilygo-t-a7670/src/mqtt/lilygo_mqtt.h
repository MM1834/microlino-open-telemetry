#pragma once
#include <Arduino.h>

void setupLilygoMqtt();
void lilygoMqttLoop();
void publishLilygoTelemetry();
String lilygoMqttStatusJson();
String lilygoMqttDebugJson();
