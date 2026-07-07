#pragma once
#include <Arduino.h>

void setupLilygoCan();
void lilygoCanLoop();

bool lilygoCanReady();
String lilygoCanStatusJson();
String lilygoCanFramesJson();
