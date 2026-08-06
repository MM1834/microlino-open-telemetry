#pragma once

#include <Arduino.h>
#include <stdint.h>

enum DecoderProfile : uint8_t {
    DECODER_PROFILE_DISPLAY_CAN = 0,
    // Keep value 2 for compatibility with devices that already stored the
    // former generic Standard-CAN template.
    DECODER_PROFILE_STANDARD_CAN_V1_PIONEER = 2,
    DECODER_PROFILE_STANDARD_CAN_V2 = 3,
    DECODER_PROFILE_DISABLED = 255
};

struct DecoderProfileDescriptor {
    DecoderProfile id;
    const char *key;
    const char *name;
    const char *description;
    bool implemented;
};

size_t decoderProfileCount();
const DecoderProfileDescriptor &decoderProfileAt(size_t index);
const DecoderProfileDescriptor *decoderProfileFind(DecoderProfile profile);
DecoderProfile decoderProfileNormalize(int value, DecoderProfile fallback = DECODER_PROFILE_DISPLAY_CAN);
const char *decoderProfileName(DecoderProfile profile);
const char *decoderProfileKey(DecoderProfile profile);
bool decoderProfileImplemented(DecoderProfile profile);
