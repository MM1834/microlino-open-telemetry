#pragma once

#include <Arduino.h>

#include "can/can_types.h"

void c6VcuDiagnosticObserve(size_t channel, const MotCanFrame &frame);
void c6VcuDiagnosticReset();
void c6VcuDiagnosticDump();
