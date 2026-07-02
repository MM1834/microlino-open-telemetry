#include "decoder_engine.h"
#include "decoder_display_can.h"

void decoderEngineHandleFrame(const MotCanFrame &frame)
{
    // v0.9.x: Display CAN is the common base decoder for all supported Microlino models.
    decoderDisplayCanHandleFrame(frame);
}
