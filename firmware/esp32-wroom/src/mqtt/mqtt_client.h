#pragma once

void setupMqtt();
void mqttLoop();
void publishTelemetry();
bool mqttTransportConnected();
