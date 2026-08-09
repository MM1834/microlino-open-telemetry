#include "device_id.h"

String motDeviceShortId()
{
    uint64_t mac = ESP.getEfuseMac();
    char buf[7];
    // ESP.getEfuseMac() stores the displayed MAC/EUI-64 bytes in reverse
    // significance. The low bits contain the shared vendor prefix and ESP32-C6
    // additionally inserts FF:FE in the middle. Use the upper 24 bits, which
    // correspond to the three device-specific base-MAC bytes in reverse order.
    snprintf(buf, sizeof(buf), "%06X", (uint32_t)((mac >> 40) & 0xFFFFFF));
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
