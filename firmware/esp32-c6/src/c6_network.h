#pragma once

#include <Arduino.h>

void c6NetworkSetup();
void c6NetworkLoop();
bool c6NetworkOnline();
String c6NetworkIp();
int c6NetworkRssi();
String c6NetworkStatus();
String c6NetworkProfileName();
String c6NetworkStateName();
String c6NetworkReason();
bool c6NetworkHomeConfigured();
bool c6NetworkMobileConfigured();
bool c6NetworkApActive();
String c6NetworkApSsid();
String c6NetworkApPassword();
