#pragma once

#include <Arduino.h>

struct C6BoardNetworkDisplaySnapshot {
    bool voltageValid;
    bool boundsValid;
    bool historyValid;
    bool lowAlarmQualified;
    bool highAlarmQualified;
    uint16_t voltageCentiV;
    uint16_t minimumCentiV;
    uint16_t maximumCentiV;
    uint16_t sourceAgeSeconds;
    uint8_t historyCount;
    uint8_t historyCadenceMinutes;
    uint16_t historyCentiV[12];
};

void c6Bm2DiagSetup();
void c6Bm2DiagLoop();
bool c6Bm2DiagStartScan();
void c6Bm2DiagStopScan();
bool c6Bm2DiagConnect();
bool c6Bm6DiagConnect();
void c6Bm2DiagDisconnect();
bool c6Bm2DiagRequestHistory();
void c6Bm2DiagDumpHistory();
String c6Bm2DiagStatusJson();
String c6Bm2PairingJson();
bool c6Bm2PairingStart();
bool c6Bm2PairingBind(const String &address, const String &protocol, String &error);
void c6Bm2PairingUnbind();
void c6Bm2DiagPrintStatus();
C6BoardNetworkDisplaySnapshot c6Bm2DiagDisplaySnapshot();
