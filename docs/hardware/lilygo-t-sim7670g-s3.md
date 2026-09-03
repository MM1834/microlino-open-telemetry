# LilyGO T-SIM7670G-S3-Standard pilot

> **Status:** Build-qualified candidate; bench and vehicle acceptance open

This target is the N16R2 Standard board with ESP32-S3-WROOM-1, 16 MB flash,
2 MB QSPI PSRAM and SIM7670G-MNGV. It reuses the complete LilyGO feature line:
local portal, OTA, WiFi-preferred/LTE-fallback AWS IoT, MQTT TLS, telemetry,
ABRP (WiFi only), GPS and optional offline cache.

## Header allocation

| Function | ESP32-S3 pins | Requirement |
|---|---|---|
| Modem UART | TX 4, RX 5 | Board internal |
| Modem control | RI 6, DTR 7, PWRKEY 46, power mode 42 | Board internal |
| Integrated GNSS NMEA | RX 48, TX 45; PPS 17 unused | Board internal |
| CAN1 TWAI | RX 39, TX 40 | External 3.3 V SN65HVD230, receive-only wiring |
| CAN2 MCP2515 | SCK 12, MOSI 11, MISO 13, CS 10, INT 14 | Adafruit MCP2515 FeatherWing, 16 MHz |

CAN1 is the Standard CAN bus and CAN2 the Display CAN bus. The camera and SD
interfaces cannot be used with this allocation. CAN1 must implement the project's
normally-open TX service jumper. For the MCP2515, `RST` and `SLNT` are held at
3.3 V and `TERM` remains open unless termination is explicitly required.

## Firmware and identity

Build environment: `T-SIM7670G-S3-Standard-AWS`. Its 16 MB partition table has
two 5 MB OTA application slots and a 6,016 KiB LittleFS partition. The offline
cache remains opt-in and is bounded to 256 KiB. Fresh boards default to device
prefix `mot-sim7670-` and vehicle ID `pioneer-sim7670`, avoiding collision with
the existing `pioneer-lilygo` pilot.

Do not flash a WROVER/A7670 image to this board. The local OTA header guard accepts
ESP32-S3/16 MB images and rejects mismatched targets or flash sizes.

## Acceptance still required

- factory flash and authenticated local portal;
- WiFi AWS connection and WiFi-to-LTE failover with a provisioned SIM/APN;
- SIM7670 modem-filesystem certificate upload and MQTT TLS reconnect;
- integrated GNSS detection/fix and modem recovery interaction;
- CAN1/CAN2 receive-only operation in the vehicle;
- cache power-loss/replay and OTA configuration preservation.
