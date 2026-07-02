#include "mqtt_topics.h"

String motTopic(const String &vehicleId, const String &suffix)
{
    return "mot/" + vehicleId + "/" + suffix;
}
