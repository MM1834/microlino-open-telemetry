#pragma once

#include <Arduino.h>

void c6LocalMqttSetup();
void c6LocalMqttLoop();
void c6LocalMqttPrepareNetwork();
void c6LocalMqttDisconnect();
bool c6LocalMqttUsesExclusiveTlsWindows();
bool c6LocalMqttNeedsExclusiveTlsWindow();
bool c6LocalMqttExclusiveTlsCooldownActive();
bool c6LocalMqttConnected();
String c6LocalMqttStatusJson();
bool c6LocalMqttCredentialsReady();
bool c6LocalMqttStoreCredentials(
    const String &rootCa,
    const String &password,
    const String &clientCertificate,
    const String &clientPrivateKey,
    String &error);
void c6LocalMqttClearCredentials();
