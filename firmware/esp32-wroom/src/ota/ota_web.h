#pragma once

#include <WebServer.h>

// Registers the Web OTA routes on the existing ESP32 WebServer instance.
// Routes:
//   GET  /update       - upload form
//   POST /update       - firmware upload
//   GET  /ota          - alias for /update
void setupOtaRoutes(WebServer &server);

// Must be called regularly from the main web UI loop.
void otaWebLoop();
