# Firmware Structure

MOT separates platform-independent logic from board-specific hardware integration.

## Common

`firmware/common/` contains:

- telemetry model
- CAN frame types
- decoder engine
- display CAN decoder
- JSON serialization
- MQTT topic helpers
- version/device ID helpers

Common code must not depend on WiFi, WebServer, TWAI setup, LTE, GPS, board pins, or storage backends.

## ESP32-WROOM

`firmware/esp32-wroom/` contains:

- PlatformIO project
- board pin configuration
- WiFi/AP fallback
- MQTT client
- Web UI
- TWAI CAN input
- board-specific `main.cpp`

## Design Rule

Common decoders only write into `MotTelemetry`.
Outputs such as MQTT, JSON API, Dashboard and ABRP read from `MotTelemetry`.
