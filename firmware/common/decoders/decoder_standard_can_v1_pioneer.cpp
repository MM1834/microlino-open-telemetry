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
    if (!frame.extended && frame.id == 0x18D && frame.dlc == 8) {
        // Pioneer V1's display-correlated SOC is carried by 0x48D byte 7.
        // 0x18D byte 7 has been observed fixed at 100 while the cockpit and
        // 0x48D report a lower SOC, so it must not become canonical SOC.
        telemetry.bms.standardSocValid = false;
        telemetry.bms.socPercent = NAN;
    }
}
