#include "mqtt_topics.h"

static String trimSlash(String s)
{
    while (s.startsWith("/")) s.remove(0, 1);
    while (s.endsWith("/")) s.remove(s.length() - 1);
    return s;
}

String motTopic(const String &prefix, const char *suffix)
{
    String p = trimSlash(prefix);
    String s = trimSlash(String(suffix));

    if (p.length() == 0) {
        p = "mot/default";
    }

    if (s.length() == 0) {
        return p;
    }

    return p + "/" + s;
}

String motDefaultPrefix(const String &vehicleId)
{
    String id = vehicleId;
    id.trim();
    id.toLowerCase();
    id.replace(" ", "-");

    if (id.length() == 0) {
        id = "default";
    }

    return "mot/" + id;
}
