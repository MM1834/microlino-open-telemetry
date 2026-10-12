#include "device_id.h"

String motDeviceShortId()
{
    uint64_t mac = ESP.getEfuseMac();
    char buf[7];
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-C3 exposes the six-byte base MAC in reverse byte order inside the
    // low 48 bits. Preserve the displayed device-specific suffix.
    const uint32_t suffix = static_cast<uint32_t>((mac >> 24) & 0xFFFFFF);
    snprintf(buf, sizeof(buf), "%02X%02X%02X",
             suffix & 0xFF, (suffix >> 8) & 0xFF, (suffix >> 16) & 0xFF);
#else
    // ESP.getEfuseMac() stores the displayed MAC/EUI-64 bytes in reverse
    // significance. The low bits contain the shared vendor prefix and ESP32-C6
    // additionally inserts FF:FE in the middle. Use the upper 24 bits, which
    // correspond to the three device-specific base-MAC bytes in reverse order.
    snprintf(buf, sizeof(buf), "%06X", (uint32_t)((mac >> 40) & 0xFFFFFF));
#endif
    return String(buf);
}

String motDeviceId()
{
    return "MOT-" + motDeviceShortId();
}

String motHostname()
{
    String h = "mot-" + motDeviceShortId();
    h.toLowerCase();
    return h;
}

String motFallbackApSsid()
{
    return motDeviceId();
}
