#pragma once

#include <Arduino.h>
#include <Client.h>

void setupLilygoModem();

bool lilygoGprsConnected();
bool lilygoEnsureGprsConnected();

String lilygoLteIp();
String lilygoModemStatusJson();
String lilygoLteDebugJson();

bool lilygoLteTcpOpen(const String& host, uint16_t port);
int lilygoLteTcpWrite(const uint8_t* data, size_t len);
int lilygoLteTcpAvailable();
int lilygoLteTcpRead(uint8_t* buffer, size_t len);
bool lilygoLteTcpConnected();
void lilygoLteTcpClose();

String lilygoLteTcpTestJson(const String& host, uint16_t port);
String lilygoLteRxDebugJson();

Client* lilygoTinyGsmClient();
Client* lilygoTinyGsmSecureClient();
String lilygoTinyGsmTraceJson();
void lilygoTinyGsmTraceClear();

void lilygoModemLoop();
