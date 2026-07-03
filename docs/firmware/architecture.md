# Firmware Architecture

The firmware is structured around a central telemetry model.

```text
CAN input → Decoder → Telemetry → MQTT / Web UI / Dashboard
```

Main responsibilities:
- WiFi setup and fallback AP
- CAN reception
- Microlino display CAN decoding
- MQTT publishing
- Web configuration
- OTA update
