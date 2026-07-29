#pragma once

#define MOT_NAME "Microlino Open Telemetry"
#define MOT_SHORT_NAME "MOT"
#define MOT_SPRINT "SPR-0004B.6"
#ifdef MOT_AWS_IOT
#define MOT_BUILD_VARIANT "AWS"
#define MOT_VERSION "SPR-0004B.6-AWS"
#else
#define MOT_BUILD_VARIANT "MQTT"
#define MOT_VERSION "SPR-0004B.6-MQTT"
#endif
#if defined(MOT_BOARD_LILYGO_T_A7670)
#define MOT_BOARD "lilygo-t-a7670"
#else
#define MOT_BOARD "esp32-wroom"
#endif
