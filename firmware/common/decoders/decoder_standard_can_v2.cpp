#include "decoder_standard_can_v2.h"
#include "decoder_standard_can_bms.h"

void decoderStandardCanV2HandleFrame(const MotCanFrame &frame)
{
    MotStandardCanBms::handleFrame(frame);
}
