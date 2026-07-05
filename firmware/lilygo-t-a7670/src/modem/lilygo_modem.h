#pragma once
#include <Arduino.h>

void setupLilygoModem();
void lilygoModemLoop();

bool lilygoModemReady();
bool lilygoNetworkRegistered();
bool lilygoGprsConnected();
bool lilygoEnsureGprsConnected();

String lilygoModemStatusJson();
String lilygoLteIp();

bool lilygoLteTcpOpen(const String& host, uint16_t port);
int lilygoLteTcpWrite(const uint8_t* data, size_t len);
int lilygoLteTcpAvailable();
int lilygoLteTcpRead(uint8_t* out, size_t len);
void lilygoLteTcpClose();
bool lilygoLteTcpConnected();

