#pragma once

#define MOT_NAME "Microlino Open Telemetry"
#define MOT_SHORT_NAME "MOT"
#define MOT_SPRINT "SPR-0004B.5R3"
#ifdef MOT_AWS_IOT
#define MOT_BUILD_VARIANT "AWS"
#define MOT_VERSION "SPR-0004B.5R3-AWS"
#else
#define MOT_BUILD_VARIANT "MQTT"
#define MOT_VERSION "SPR-0004B.5R3-MQTT"
#endif
#define MOT_BOARD "esp32-wroom"
