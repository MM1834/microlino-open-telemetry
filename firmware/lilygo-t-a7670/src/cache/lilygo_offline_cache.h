#pragma once

#include <Arduino.h>

class MotAwsIotClient;

void lilygoOfflineCacheSetup();
void lilygoOfflineCacheLoop(MotAwsIotClient &client, bool awsConnected, bool freshLivePublished);
void lilygoOfflineCacheHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length);
void lilygoOfflineCachePurge();
String lilygoOfflineCacheStatusJson();
