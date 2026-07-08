# ESP32-WROOM and CAN485 Architecture

The ESP32-WROOM / CAN485 target is the original Microlino Open Telemetry firmware.

## Responsibilities

- Connect to WiFi
- Read Microlino display CAN
- Decode telemetry
- Publish MQTT
- Send ABRP telemetry
- Serve Web UI and JSON APIs
- Provide OTA update
- Provide configuration backup / restore

## CAN

The ESP32-WROOM firmware uses the ESP32 TWAI controller with an external CAN transceiver.

The current WROOM firmware focuses on display CAN input. OBD2 is intentionally not part of the current release.

## Network

The WROOM target uses WiFi only.

```text
WiFi connected
  -> MQTT
  -> ABRP
  -> OTA / Web UI
```

## ABRP

For WROOM, no GPS module is assumed. Latitude and longitude are not sent unless a valid GPS source exists. ABRP can use the location from the ABRP mobile app.
