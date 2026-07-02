#include "mot_app.h"
#include "../telemetry/telemetry.h"
#include "../system/version.h"
#include "../system/device_id.h"

#include <Arduino.h>

void motAppSetup()
{
    telemetryInit();
    telemetry.system.firmwareVersion = MOT_VERSION;
    telemetry.system.deviceId = motDeviceId();

    Serial.println();
    Serial.println("========================================");
    Serial.println(MOT_NAME);
    Serial.printf("Version : %s\n", MOT_VERSION);
    Serial.printf("Device  : %s\n", telemetry.system.deviceId.c_str());
    Serial.println("========================================");
}

void motAppLoop()
{
    telemetryUpdateSystemRuntime();
}
