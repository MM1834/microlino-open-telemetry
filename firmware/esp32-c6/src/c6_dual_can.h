#pragma once

#include <Arduino.h>
#include "decoders/decoder_profile.h"

struct C6CanChannelStatus {
    bool started = false;
    uint32_t frames = 0;
    uint32_t lastFrameMs = 0;
    uint32_t receiveErrors = 0;
    DecoderProfile profile = DECODER_PROFILE_DISABLED;
};

bool c6DualCanSetup();
void c6DualCanLoop();
bool c6DualCanSetProfile(size_t channel, DecoderProfile profile);
const C6CanChannelStatus &c6CanStatus(size_t channel);
