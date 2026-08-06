#pragma once

#include "can/can_types.h"

void c6CanScanReset();
void c6CanScanObserve(size_t channel, const MotCanFrame &frame);
void c6CanScanDump();
