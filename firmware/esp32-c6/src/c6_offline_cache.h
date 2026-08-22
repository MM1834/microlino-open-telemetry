#pragma once

#include <Arduino.h>

class MotAwsIotClient;

void c6OfflineCacheSetup();
void c6OfflineCacheLoop(
    MotAwsIotClient &client,
    bool awsConnected,
    bool freshLivePublished
);
void c6OfflineCacheHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length);
void c6OfflineCachePurge();
String c6OfflineCacheStatusJson();
