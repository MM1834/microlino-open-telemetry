#pragma once

#include <Arduino.h>
#include <WebServer.h>

struct LocalOtaOptions {
    String adminPassword;
    bool enabled = false;
    String firmwareLabel;
};

void localOtaSetup(WebServer &server, const LocalOtaOptions *options);
void localOtaLoop();
