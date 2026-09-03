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
    if (!frame.extended && frame.id == 0x48D && frame.dlc == 8) {
        telemetry.bms.pioneerSocValid = true;
        telemetry.bms.socInternal = frame.data[6];
        telemetry.bms.socDisplay = frame.data[7];
        telemetry.bms.pioneerSocLastUpdateMs = millis();
        return;
    }
    MotStandardCanBms::handleFrame(frame, PIONEER_RULES);
}
