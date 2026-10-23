#pragma once

#include <Arduino.h>

class MotAwsIotClient;

struct C6ChargeDisplaySnapshot {
    bool active = false;
    bool energyValid = false;
    uint32_t energyChargedWh = 0;
    bool completedValid = false;
    bool completedSocValid = false;
    bool completedEnergyValid = false;
    bool completedDurationValid = false;
    uint8_t completedStartSocPercent = 0;
    uint8_t completedEndSocPercent = 0;
    uint16_t completedDurationMinutes = 0;
    uint32_t completedEnergyChargedWh = 0;
    uint16_t completedSequence = 0;
};

void c6ChargeJournalSetup();
void c6ChargeJournalLoop();
void c6ChargeBackfillLoop(MotAwsIotClient &client, bool connected, bool freshLivePublished);
void c6ChargeBackfillHandleAwsMessage(char *topic, uint8_t *payload, unsigned int length);
void c6ChargeJournalPurge();
String c6ChargeJournalStatusJson();
C6ChargeDisplaySnapshot c6ChargeDisplaySnapshot();
