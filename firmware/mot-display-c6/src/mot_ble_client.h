#pragma once

#include <Arduino.h>

#include "display/mot_display_ble_protocol.h"

void motBleClientSetup();
void motBleClientLoop();
bool motBleClientLatest(MotDisplayBle::TelemetryV1 &packet, uint32_t &receivedAgeMs);
bool motBleClientLatestStatus(MotDisplayBle::StatusV1 &packet, uint32_t &receivedAgeMs);
bool motBleClientLatestExtended(MotDisplayBle::ExtendedTelemetryV1 &packet, uint32_t &receivedAgeMs);
bool motBleClientLatestDriving(MotDisplayBle::DrivingStatusV1 &packet, uint32_t &receivedAgeMs);
const char *motBleClientStatus();
bool motBleClientIsBound();
void motBleClientForgetPairing();
