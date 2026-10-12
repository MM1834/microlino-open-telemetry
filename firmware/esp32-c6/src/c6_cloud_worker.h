#pragma once

#include <Arduino.h>

void c6CloudWorkerSetup();
void c6CloudWorkerLoop();
String c6CloudWorkerStatusJson();
void c6CloudWorkerRequestAbrpPause();
bool c6CloudWorkerAbrpPauseReady();
void c6CloudWorkerReleaseAbrpPause();
