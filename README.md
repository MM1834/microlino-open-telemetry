# Microlino Open Telemetry (MOT)

Open Source telemetry platform for Microlino vehicles based on ESP32 hardware.

## Highlights

- CAN bus decoding
- MQTT telemetry
- Local WebUI
- Browser dashboard (desktop & mobile)
- OTA firmware updates
- Backup & Restore
- GPS support (LilyGO)
- Experimental LTE transport

## Supported hardware

| Platform | Status |
|---|---|
| ESP32-WROOM + WeAct CAN485 | Stable |
| LilyGO T-A7670G | Working |
| LTE MQTT | Experimental |

## Quick start

1. Flash the firmware for your board.
2. Connect to the setup Access Point.
3. Configure WiFi and MQTT.
4. Verify telemetry in the dashboard.
5. Export a backup.

## Documentation

- `docs/index.md` – documentation landing page
- Hardware Guide
- WebUI Guide
- Firmware Guide
- Developer Guide

## Current priorities

- LTE MQTT stability
- Security hardening
- MQTT heartbeat / Last Will
- TLS / AWS IoT evaluation

## License

See the repository license.
