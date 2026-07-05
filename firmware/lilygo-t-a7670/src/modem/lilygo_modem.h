#pragma once
#include <Arduino.h>
void setupLilygoModem(); void lilygoModemLoop(); bool lilygoModemReady(); bool lilygoNetworkRegistered(); bool lilygoGprsConnected(); String lilygoModemStatusJson();
