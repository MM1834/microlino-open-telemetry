#include "local_web_security.h"

namespace LocalWebSecurity {
String generateRecoveryPassword()
{
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    String value = "mot-";
    for (size_t i = 0; i < 16; ++i) value += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    return value;
}

bool authenticate(WebServer &server, const String &password)
{
    if (password.length() < 12) {
        server.send(503, "text/plain", "Local admin setup required at /setup");
        return false;
    }
    if (!server.authenticate("admin", password.c_str())) {
        server.requestAuthentication();
        return false;
    }
    return true;
}

bool requireSameOrigin(WebServer &server)
{
    const String host = server.hostHeader();
    const String origin = server.header("Origin");
    const String referer = server.header("Referer");
    if (origin == "http://" + host || origin == "https://" + host ||
        referer.startsWith("http://" + host + "/") ||
        referer.startsWith("https://" + host + "/")) return true;
    server.send(403, "text/plain", "Same-origin request required");
    return false;
}

void collectSecurityHeaders(WebServer &server)
{
    static const char *headers[] = {"Origin", "Referer"};
    server.collectHeaders(headers, 2);
}
}
