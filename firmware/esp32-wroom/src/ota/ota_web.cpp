#include "ota_web.h"

#include "../app_config.h"
#include "system/device_id.h"
#include "system/version.h"
#include "web/local_ota.h"

namespace {
LocalOtaOptions options;
}

void setupOtaRoutes(WebServer &server)
{
    options.adminPassword = config.otaPassword;
    options.enabled = config.otaEnabled;
    options.firmwareLabel = String(MOT_VERSION) + " · " + MOT_BOARD + " · " + motDeviceId();
    localOtaSetup(server, &options);
}

void otaWebLoop()
{
    localOtaLoop();
}
