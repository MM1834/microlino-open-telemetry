#pragma once

#include "can/can_types.h"

void c6DriveCaptureReset();
void c6DriveCaptureObserve(size_t channel, const MotCanFrame &frame);
void c6DriveCaptureDump();
void c6DriveCaptureTraceDump();
