#pragma once
#include <Arduino.h>

void setupL76kGps();
void l76kGpsLoop();

bool l76kGpsValid();
double l76kLatitude();
double l76kLongitude();
double l76kSpeedKmph();
uint32_t l76kSatellites();
double l76kHdop();
uint32_t l76kLocationAgeMs();

String l76kGpsStatusJson();
