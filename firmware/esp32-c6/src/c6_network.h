#pragma once

#include <Arduino.h>

void c6NetworkSetup();
void c6NetworkLoop();
bool c6NetworkOnline();
String c6NetworkIp();
int c6NetworkRssi();
String c6NetworkStatus();
