#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace LocalWebSecurity {
String generateRecoveryPassword();
bool authenticate(WebServer &server, const String &password);
bool requireSameOrigin(WebServer &server);
void collectSecurityHeaders(WebServer &server);
}
