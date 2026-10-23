#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class MotAwsIotClient;

struct C6JourneyDisplaySnapshot {
    bool active = false;
    bool distanceValid = false;
    bool consumptionValid = false;
    float distanceKm = 0.0f;
    uint32_t durationSeconds = 0;
    float netKwhPer100Km = 0.0f;
    uint16_t regenPeakW = 0;
    bool altitudeValid = false;
    bool elevationGainValid = false;
    int16_t altitudeMeters = 0;
    uint16_t ascentMeters = 0;
    uint16_t descentMeters = 0;
    uint32_t altitudeAgeMs = 0;
};

void c6JourneyEnergySetup();
void c6JourneyEnergyLoop();
bool c6JourneyEnergyAddToEnvelope(JsonObject values);
void c6JourneyEnergyEnvelopePublished();
void c6JourneyBackfillLoop(MotAwsIotClient &client, bool connected, bool freshLivePublished);
void c6JourneyBackfillHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length);
void c6JourneyBackfillPurge();
String c6JourneyEnergyStatusJson();
C6JourneyDisplaySnapshot c6JourneyDisplaySnapshot();
