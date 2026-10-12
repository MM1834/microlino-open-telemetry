#pragma once

// Single source of truth for all firmware-visible version information.
// Update MOT_SPRINT and MOT_REVISION for every firmware-changing revision.
#define MOT_NAME "Microlino Open Telemetry"
#define MOT_SHORT_NAME "MOT"
#define MOT_SPRINT "C6-001"
#define MOT_REVISION "REV39"

#ifdef MOT_BM2_DIAGNOSTIC
#define MOT_BUILD_VARIANT "BM2-BM6-DIAG"
#define MOT_VERSION "C6-001-REV38-BM2-BM6-AUTO-DIAG"
#elif defined(MOT_DATA_ANALYSIS_BUILD)
#define MOT_BUILD_VARIANT "DATA-ANALYSIS2-B025"
#define MOT_VERSION "C6-001-REV38-DATA-ANALYSIS2-B025"
#elif defined(MOT_DIAGNOSTIC_BUILD)
#define MOT_BUILD_VARIANT "SOC-LOCK-DIAG-B025"
#define MOT_VERSION "C6-001-REV38-SOC-LOCK-DIAG-B025"
#elif defined(MOT_AWS_IOT)
#define MOT_BUILD_VARIANT "AWS"
#define MOT_VERSION "C6-001-REV39-AWS"
#else
#define MOT_BUILD_VARIANT "MQTT"
#define MOT_VERSION "C6-001-REV39-MQTT"
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
