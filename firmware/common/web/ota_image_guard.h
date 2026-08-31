#pragma once

#include <Arduino.h>

struct OtaImageGuardResult {
    bool accepted;
    String reason;
};

inline uint16_t otaRunningChipId()
{
#if defined(CONFIG_IDF_TARGET_ESP32C6)
    return 13;
#elif defined(CONFIG_IDF_TARGET_ESP32)
    return 0;
#else
#error "Unsupported ESP target for OTA image guard"
#endif
}

inline uint32_t otaImageFlashBytes(uint8_t sizeCode)
{
    if (sizeCode > 7) return 0;
    return (1UL << sizeCode) * 1024UL * 1024UL;
}

inline OtaImageGuardResult otaValidateImageHeader(const uint8_t *data, size_t length)
{
    constexpr size_t REQUIRED_HEADER_BYTES = 24;
    constexpr uint8_t ESP_IMAGE_MAGIC = 0xE9;
    if (!data || length < REQUIRED_HEADER_BYTES) {
        return {false, "Firmware image header is missing or incomplete"};
    }
    if (data[0] != ESP_IMAGE_MAGIC) {
        return {false, "File is not an Espressif application image"};
    }

    const uint16_t imageChipId = static_cast<uint16_t>(data[12]) |
        (static_cast<uint16_t>(data[13]) << 8);
    const uint16_t runningChipId = otaRunningChipId();
    if (imageChipId != runningChipId) {
        return {
            false,
            "Firmware chip mismatch (image " + String(imageChipId) +
                ", adapter " + String(runningChipId) + ")",
        };
    }

    const uint32_t imageFlashBytes = otaImageFlashBytes(data[3] >> 4);
    const uint32_t runningFlashBytes = ESP.getFlashChipSize();
    if (!imageFlashBytes || imageFlashBytes != runningFlashBytes) {
        return {
            false,
            "Firmware flash-size mismatch (image " +
                String(imageFlashBytes / (1024UL * 1024UL)) + " MB, adapter " +
                String(runningFlashBytes / (1024UL * 1024UL)) + " MB)",
        };
    }
    return {true, ""};
}
