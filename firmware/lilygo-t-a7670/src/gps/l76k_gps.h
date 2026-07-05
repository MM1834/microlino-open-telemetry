#pragma once
#include <Arduino.h>

void setupL76kGps();
void l76kGpsLoop();

bool l76kGpsValid();
double l76kLatitude();
double l76kLongitude();
double l76kSpeedKmph();

String l76kGpsStatusJson();
