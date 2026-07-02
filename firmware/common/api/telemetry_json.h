#pragma once

#include <Arduino.h>
#include "../telemetry/telemetry.h"

String telemetryToJson(const TelemetryState &state);
