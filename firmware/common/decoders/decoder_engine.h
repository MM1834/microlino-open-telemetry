#pragma once

#include "../can/can_types.h"
#include "decoder_profile.h"

void decoderEngineHandleFrame(const MotCanFrame &frame, DecoderProfile profile);
void decoderEngineHandleFrame(const MotCanFrame &frame);
