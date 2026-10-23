#pragma once

#include <Arduino.h>

#include "display/mot_display_ble_protocol.h"

void motBleClientSetup();
void motBleClientLoop();
bool motBleClientLatest(MotDisplayBle::TelemetryV1 &packet, uint32_t &receivedAgeMs);
bool motBleClientLatestStatus(MotDisplayBle::StatusV1 &packet, uint32_t &receivedAgeMs);
bool motBleClientLatestExtended(MotDisplayBle::ExtendedTelemetryV1 &packet, uint32_t &receivedAgeMs);
bool motBleClientLatestDriving(MotDisplayBle::DrivingStatusV1 &packet, uint32_t &receivedAgeMs);
bool motBleClientLatestChargeSummary(MotDisplayBle::ChargeSummaryV1 &packet,
                                     uint32_t &receivedAgeMs);
bool motBleClientLatestJourneyAltitude(MotDisplayBle::JourneyAltitudeV1 &packet,
                                       uint32_t &receivedAgeMs);
bool motBleClientLatestVehicleBoardNetwork(MotDisplayBle::VehicleBoardNetworkV1 &packet,
                                           uint32_t &receivedAgeMs);
bool motBleClientLatestSoftwareVersions(MotDisplayBle::SoftwareVersionsV1 &packet,
                                       uint32_t &receivedAgeMs);
bool motBleClientLatestPresentationConfig(MotDisplayBle::PresentationConfigV1 &packet,
                                          uint32_t &receivedAgeMs);
bool motBleClientWritePresentationConfig(uint8_t language, uint8_t unitSystem);
const char *motBleClientStatus();
bool motBleClientIsBound();
void motBleClientForgetPairing();
