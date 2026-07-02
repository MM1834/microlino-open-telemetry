#include "telemetry.h"

MotTelemetry telemetry;

void telemetryInit()
{
    telemetry = MotTelemetry();
}

void telemetryUpdateSystemRuntime()
{
    telemetry.system.uptimeSec = millis() / 1000;
}
