#pragma once

#include <Arduino.h>

class MotAwsIotClient;

struct C6ChargeDisplaySnapshot {
    bool active = false;
    bool energyValid = false;
    uint32_t energyChargedWh = 0;
};

void c6ChargeJournalSetup();
void c6ChargeJournalLoop();
void c6ChargeBackfillLoop(MotAwsIotClient &client, bool connected, bool freshLivePublished);
void c6ChargeBackfillHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length);
void c6ChargeJournalPurge();
String c6ChargeJournalStatusJson();
C6ChargeDisplaySnapshot c6ChargeDisplaySnapshot();
