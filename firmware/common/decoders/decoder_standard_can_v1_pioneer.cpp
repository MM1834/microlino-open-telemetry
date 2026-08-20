#include "decoder_standard_can_v1_pioneer.h"
#include "decoder_standard_can_bms.h"

namespace {

static constexpr MotStandardCanBms::DecoderRules PIONEER_RULES = {
    0.3f,     // confirmed amperes per raw unit
    40000,
    65000,
    12000.0f,
    25000.0f,
    2.0f,
    2.0f,
};

} // namespace

void decoderStandardCanV1PioneerHandleFrame(const MotCanFrame &frame)
{
    MotStandardCanBms::handleFrame(frame, PIONEER_RULES);
}
