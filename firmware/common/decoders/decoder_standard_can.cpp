#include "decoder_standard_can.h"

void decoderStandardCanHandleFrame(const MotCanFrame &frame)
{
    // Intentionally empty. Official Standard-CAN PIDs and scaling must be
    // supplied by Microlino before this profile is allowed to publish data.
    (void)frame;
}
