#pragma once

#include <Arduino.h>

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
void c6Bm2DiagPrintStatus();
