#pragma once

#include <Arduino.h>

void setupWroomGps();
void wroomGpsLoop();

bool wroomGpsStarted();
bool wroomGpsSeen();
bool wroomGpsValid();
double wroomGpsLatitude();
double wroomGpsLongitude();
double wroomGpsSpeedKmph();
double wroomGpsAltitudeMeters();
double wroomGpsCourseDegrees();
uint32_t wroomGpsSatellites();
double wroomGpsHdop();
uint32_t wroomGpsLocationAgeMs();
String wroomGpsStatusJson();
