#include "wroom_abrp.h"

#include "app_config.h"
#include "common/abrp/abrp_client.h"
#include "gps/wroom_gps.h"

namespace {
AbrpSettings settings()
{
    AbrpSettings value;
    value.enabled = config.abrpServiceEnabled;
    value.apiKey = config.abrpApiKey;
    value.userToken = config.abrpUserToken;
    return value;
}

bool location(AbrpLocation &value)
{
    value.valid = wroomGpsValid();
    if (value.valid) {
        value.latitude = wroomGpsLatitude();
        value.longitude = wroomGpsLongitude();
    }
    return value.valid;
}
}

void setupWroomAbrp() { setupAbrp(settings()); }
void wroomAbrpLoop() { abrpLoop(settings(), location); }
bool queueWroomAbrpTelemetry() { return queueAbrpTelemetry(settings(), location); }
String wroomAbrpStatusJson() { return abrpStatusJson(settings()); }
