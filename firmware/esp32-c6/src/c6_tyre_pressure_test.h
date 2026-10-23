#pragma once

#include <cstdint>

// Temporary, explicit adapter-side source used only to qualify the additive
// display/cloud contract before real wheel sensors and assignments are accepted.
namespace C6TyrePressureTest {
constexpr uint16_t kPressureKpa[4] = {190, 190, 170, 145};

constexpr bool alarm()
{
    for (const uint16_t pressure : kPressureKpa) {
        if (pressure < 150U || pressure > 250U) return true;
    }
    return false;
}
}  // namespace C6TyrePressureTest
