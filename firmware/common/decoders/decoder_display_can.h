#pragma once

#include "../can/can_types.h"

void decoderDisplayCanHandleFrame(const MotCanFrame &frame);
void decoderDisplayCanHandleLocalStatusFrame(const MotCanFrame &frame);
