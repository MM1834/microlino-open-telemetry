#include "decoder_standard_can_v2.h"
#include "decoder_standard_can_bms.h"

namespace {

// These V2 rules intentionally live outside the Pioneer decoder. They preserve
// the initial 0x18D/0x4AD pilot behaviour but remain provisional until compared
// with independent measurements on a V2 vehicle.
static constexpr MotStandardCanBms::DecoderRules V2_PROVISIONAL_RULES = {
    0.3f,
    40000,
    65000,
    12000.0f,
    25000.0f,
    2.0f,
    2.0f,
};

} // namespace

void decoderStandardCanV2HandleFrame(const MotCanFrame &frame)
{
    MotStandardCanBms::handleFrame(frame, V2_PROVISIONAL_RULES);
}
