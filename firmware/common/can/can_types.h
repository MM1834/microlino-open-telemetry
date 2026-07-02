#pragma once

#include <Arduino.h>

struct MotCanFrame {
    uint32_t id = 0;
    bool extended = false;
    uint8_t dlc = 0;
    uint8_t data[8] = {0};
    uint32_t receivedMs = 0;
};
