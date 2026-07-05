#pragma once
#include <Arduino.h>

void setupLilygoModem();
void lilygoModemLoop();

bool lilygoModemReady();
bool lilygoNetworkRegistered();
bool lilygoGprsConnected();
bool lilygoEnsureGprsConnected();

String lilygoModemStatusJson();
String lilygoLteIp();
