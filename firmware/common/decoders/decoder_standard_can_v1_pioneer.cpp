#include "decoder_standard_can_v1_pioneer.h"
#include "decoder_standard_can_bms.h"

void decoderStandardCanV1PioneerHandleFrame(const MotCanFrame &frame)
{
    MotStandardCanBms::handleFrame(frame);
}
