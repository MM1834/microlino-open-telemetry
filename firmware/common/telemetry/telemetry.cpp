#include "telemetry.h"

TelemetryState telemetry;

void telemetryReset()
{
    telemetry = TelemetryState();
}

bool telemetryHasVehicleData()
{
    return telemetry.display.valid || telemetry.charging.valid || telemetry.bms.valid;
}
