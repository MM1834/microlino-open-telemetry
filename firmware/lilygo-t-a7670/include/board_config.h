#pragma once

#include "system/version.h"

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

// CAN1 Display Bus via SN65HVD230. GPIO33 remains reserved for MODEM_RI_PIN.
// Firmware uses TWAI listen-only; the transceiver must also satisfy the
// independent receive-only hardware contract.
#define CAN_RX_PIN 36
#define CAN_TX_PIN 13

// Adafruit MCP2515 CAN Bus FeatherWing on the unused onboard SD SPI bus.
// No SD card may be installed. GPIO34 is input-only and used for MCP2515 INT.
// Hardware gates: TERM open and SLNT tied permanently to 3.3 V.
#define CAN2_SPI_MISO_PIN 39
#define CAN2_SPI_MOSI_PIN 32
#define CAN2_SPI_SCK_PIN 14
#define CAN2_SPI_CS_PIN 18
#define CAN2_INT_PIN 34
#define CAN2_MCP_CLOCK MCP_16MHZ

#define SETUP_AP_SSID "MOT-LilyGO"
#define SETUP_AP_PASS "microlino"
