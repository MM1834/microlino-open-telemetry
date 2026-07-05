#pragma once
#include <Arduino.h>
enum class LilygoNetworkMode { AP_ONLY, WIFI_CLIENT, LTE };
void setupLilygoNetwork(); void lilygoNetworkLoop(); bool lilygoNetworkOnline(); String lilygoNetworkModeName(); String lilygoNetworkIp(); String lilygoNetworkStatusJson();
