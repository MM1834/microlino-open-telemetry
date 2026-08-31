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

bool telemetryIsCharging()
{
    const uint32_t nowMs = millis();
    const bool freshBmsCurrent = telemetry.bms.packCurrentValid &&
        nowMs - telemetry.bms.packCurrentLastUpdateMs <= 10000;
    const bool freshBmsStatus = telemetry.bms.statusLastUpdateMs != 0 &&
        telemetry.bms.packStatusValid &&
        nowMs - telemetry.bms.statusLastUpdateMs <= 10000;
    return freshBmsCurrent && freshBmsStatus
        ? telemetry.bms.plugged && telemetry.bms.packCurrentA > 2.0f
        : telemetry.charging.isCharging;
}
