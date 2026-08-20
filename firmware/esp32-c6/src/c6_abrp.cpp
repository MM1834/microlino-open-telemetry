#include "c6_abrp.h"

#include "abrp/abrp_client.h"
#include "c6_config.h"
#include "c6_aws.h"
#include "c6_gps.h"
#include "c6_network.h"

namespace {
AbrpSettings settings()
{
    AbrpSettings value;
    value.enabled = c6Config.abrpEnabled;
    value.apiKey = c6Config.abrpApiKey;
    value.userToken = c6Config.abrpUserToken;
    return value;
}

bool location(AbrpLocation &value)
{
    value.valid = c6GpsValid();
    if (value.valid) {
        value.latitude = c6GpsLatitude();
        value.longitude = c6GpsLongitude();
    }
    return value.valid;
}
}

void c6AbrpSetup() { setupAbrp(settings()); }
void c6AbrpLoop()
{
    // AWS is the primary telemetry transport. Do not start a second TLS
    // session while MQTT is reconnecting or recovering memory.
    if (c6NetworkTransportReady() && c6AwsConnected()) abrpLoop(settings(), location);
}
bool c6AbrpQueueTelemetry()
{
    return c6NetworkTransportReady() && c6AwsConnected() && queueAbrpTelemetry(settings(), location);
}
String c6AbrpStatusJson() { return abrpStatusJson(settings()); }
bool c6AbrpConfigured() { return abrpEnabled(settings()); }
