#pragma once

#define MOT_NAME "Microlino Open Telemetry"
#define MOT_VERSION "1.1.0-lilygo-sprint2"
#define MOT_BOARD "lilygo-t-a7670"

// LilyGO T-A7670G R2 / T-A7670X-GPS V1.1 2024-04-26
// MCU: ESP32-WROVER
// Modem: SIMCom A7670G-LLSE

#define LILYGO_T_A7670 1
#define LILYGO_GPS_SHIELD 1

// Modem UART / control pins
#define MODEM_RX_PIN 27
#define MODEM_TX_PIN 26
#define MODEM_PWR_PIN 4
#define MODEM_PWRKEY_PIN MODEM_PWR_PIN
#define BOARD_POWER_ON_PIN 12
#define MODEM_RST_PIN 5
#define MODEM_RESET_PIN MODEM_RST_PIN
#define MODEM_DTR_PIN 25
#define MODEM_RI_PIN 33
#define MODEM_BAUD 115200

// External L76K GPS on Serial2
#define GPS_RX_PIN 22
#define GPS_TX_PIN 21
#define GPS_PPS_PIN 23
#define GPS_WAKEUP_PIN 19
#define GPS_BAUD 9600

// Swisscom APN used for Sprint 2 LTE test
#define LTE_APN "gprs.swisscom.ch"
#define LTE_USER ""
#define LTE_PASS ""

// Planned CAN Display Bus via SN65HVD230.
// NOTE: GPIO33 is MODEM_RI_PIN on this board, so CAN TX=33 is not enabled yet.
// CAN remains intentionally disabled in LilyGO Sprint 2.
#define CAN_RX_PIN 32
#define CAN_TX_PIN 13

#define SETUP_AP_SSID "MOT-LilyGO"
#define SETUP_AP_PASS "microlino"
