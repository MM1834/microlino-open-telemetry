#pragma once

// Single source of truth for all firmware-visible version information.
// Update MOT_SPRINT and MOT_REVISION for every firmware-changing revision.
#define MOT_NAME "Microlino Open Telemetry"
#define MOT_SHORT_NAME "MOT"
#define MOT_SPRINT "C6-001"
#define MOT_REVISION "REV15"

#ifdef MOT_DIAGNOSTIC_BUILD
#define MOT_BUILD_VARIANT "SOC-DIAG-B025"
#define MOT_VERSION "C6-001-REV14-SOC-DIAG-B025"
#elif defined(MOT_AWS_IOT)
#define MOT_BUILD_VARIANT "AWS"
#define MOT_VERSION "C6-001-REV14-AWS"
#else
#define MOT_BUILD_VARIANT "MQTT"
#define MOT_VERSION "C6-001-REV14-MQTT"
#endif

#ifndef MOT_BOARD
#if defined(MOT_BOARD_LILYGO_T_A7670)
#define MOT_BOARD "lilygo-t-a7670"
#else
#define MOT_BOARD "esp32-wroom"
#endif
#endif

#define MOT_BUILD_DATE __DATE__
#define MOT_BUILD_TIME __TIME__
