#include "decoder_profile.h"

static const DecoderProfileDescriptor PROFILES[] = {
    {
        DECODER_PROFILE_DISPLAY_CAN,
        "display-can",
        "Microlino Display CAN",
        "Production profile using the currently decoded Display-CAN frames.",
        true
    },
    {
        DECODER_PROFILE_STANDARD_CAN,
        "standard-can",
        "Microlino Standard CAN (template)",
        "Prepared profile without guessed PIDs. Add official Microlino definitions here later.",
        false
    },
    {
        DECODER_PROFILE_DISABLED,
        "disabled",
        "Disabled / unused",
        "Do not decode frames received on this CAN input.",
        true
    }
};

size_t decoderProfileCount()
{
    return sizeof(PROFILES) / sizeof(PROFILES[0]);
}

const DecoderProfileDescriptor &decoderProfileAt(size_t index)
{
    if (index >= decoderProfileCount()) index = 0;
    return PROFILES[index];
}

const DecoderProfileDescriptor *decoderProfileFind(DecoderProfile profile)
{
    for (size_t i = 0; i < decoderProfileCount(); ++i) {
        if (PROFILES[i].id == profile) return &PROFILES[i];
    }
    return nullptr;
}

DecoderProfile decoderProfileNormalize(int value, DecoderProfile fallback)
{
    DecoderProfile candidate = static_cast<DecoderProfile>(value);
    return decoderProfileFind(candidate) ? candidate : fallback;
}

const char *decoderProfileName(DecoderProfile profile)
{
    const DecoderProfileDescriptor *descriptor = decoderProfileFind(profile);
    return descriptor ? descriptor->name : "Unknown CAN profile";
}

const char *decoderProfileKey(DecoderProfile profile)
{
    const DecoderProfileDescriptor *descriptor = decoderProfileFind(profile);
    return descriptor ? descriptor->key : "unknown";
}

bool decoderProfileImplemented(DecoderProfile profile)
{
    const DecoderProfileDescriptor *descriptor = decoderProfileFind(profile);
    return descriptor && descriptor->implemented;
}
