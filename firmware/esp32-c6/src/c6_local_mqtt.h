#pragma once

#include <Arduino.h>

void c6LocalMqttSetup();
void c6LocalMqttLoop();
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
