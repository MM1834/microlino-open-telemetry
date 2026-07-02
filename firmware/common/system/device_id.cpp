#include "device_id.h"

String motDeviceShortId()
{
    uint64_t mac = ESP.getEfuseMac();
    char buf[7];
    snprintf(buf, sizeof(buf), "%06X", (uint32_t)(mac & 0xFFFFFF));
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
