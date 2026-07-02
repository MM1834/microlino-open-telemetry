#pragma once

#include <Arduino.h>

enum class NetworkState {
    STA,
    FALLBACK_AP,
    LTE
};

void setupNetwork();
bool networkOnline();
String networkIp();
String networkModeName();
int networkRssi();
NetworkState networkState();
