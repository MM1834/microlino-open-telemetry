#pragma once

#include <Arduino.h>

void c6AwsSetup();
void c6AwsLoop();
void c6AwsPrepareNetwork();
void c6AwsDisconnect();
String c6AwsStatus();
bool c6AwsConnected();
bool c6AwsRecentlyHealthy();
bool c6AwsAllowsAbrp();
bool c6AwsReadyForTlsPause();
