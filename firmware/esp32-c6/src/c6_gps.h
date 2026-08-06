#pragma once

#include <Arduino.h>

void c6GpsSetup();
void c6GpsLoop();
String c6GpsState();
bool c6GpsDetected();
bool c6GpsValid();
bool c6GpsSeen();
uint64_t c6GpsChars();
String c6GpsMessage();
double c6GpsLatitude();
double c6GpsLongitude();
double c6GpsSpeedKmph();
uint32_t c6GpsSatellites();
