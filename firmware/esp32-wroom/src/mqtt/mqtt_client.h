#pragma once

#include "MqttDiagnostics.h"

void setupMqtt();
void mqttLoop();
void publishTelemetry();
bool mqttTransportConnected();
MqttDiagResult mqttTransportDiagnostics();
