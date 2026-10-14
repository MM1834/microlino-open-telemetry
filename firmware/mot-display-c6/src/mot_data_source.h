#pragma once

#include <Arduino.h>

#include "display/mot_display_ble_protocol.h"

void motDataSourceSetup();
void motDataSourceLoop();
bool motDataSourceStandalone();
bool motDataSourceLatest(MotDisplayBle::TelemetryV1 &packet, uint32_t &ageMs);
bool motDataSourceLatestStatus(MotDisplayBle::StatusV1 &packet, uint32_t &ageMs);
bool motDataSourceLatestExtended(MotDisplayBle::ExtendedTelemetryV1 &packet, uint32_t &ageMs);
bool motDataSourceLatestDriving(MotDisplayBle::DrivingStatusV1 &packet, uint32_t &ageMs);
const char *motDataSourceStatus();
void motDataSourceForgetPairing();
uint32_t motDataSourceCanFrames();
