#include "decoder_engine.h"
#include "decoder_display_can.h"
#include "decoder_standard_can_v1_pioneer.h"
#include "decoder_standard_can_v2.h"

void decoderEngineHandleFrame(const MotCanFrame &frame, DecoderProfile profile)
{
    switch (profile) {
        case DECODER_PROFILE_DISPLAY_CAN:
            decoderDisplayCanHandleFrame(frame);
            break;
        case DECODER_PROFILE_STANDARD_CAN_V1_PIONEER:
            decoderStandardCanV1PioneerHandleFrame(frame);
            break;
        case DECODER_PROFILE_STANDARD_CAN_V2:
            decoderStandardCanV2HandleFrame(frame);
            break;
        case DECODER_PROFILE_DISABLED:
        default:
            break;
    }
}

void decoderEngineHandleFrame(const MotCanFrame &frame)
{
    // Backward-compatible default for callers not yet providing a profile.
    decoderEngineHandleFrame(frame, DECODER_PROFILE_DISPLAY_CAN);
}
