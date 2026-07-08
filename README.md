# Microlino Open Telemetry

Microlino Open Telemetry (MOT) is an ESP32-based telemetry platform for the Microlino. It reads vehicle data via CAN, enriches it with GPS and system status, and publishes telemetry through MQTT over WiFi or LTE.

> TODO: Add project hero image: `docs/images/hardware/mot-installed.jpg`

## Current release status

Stable firmware baseline: `v1.1.0-lilygo-stability`

## Features

- CAN receive via ESP32 TWAI and SN65HVD230
- MQTT telemetry over WiFi or LTE
- LilyGO T-A7670G LTE support
- External L76K GPS support
- Local WebUI and REST status API
- OTA firmware update
- JSON Backup/Restore
- Factory Reset
- CAN diagnostics
- Modem recovery for the A7670G

## Supported hardware

| Hardware | Status | Notes |
|---|---:|---|
| ESP32-WROOM + SN65HVD230 | Supported | Classic MOT ESP32 setup |
| WeAct Studio ESP32 CAN485 | Supported | Compatible CAN pins with SN65HVD230 |
| LilyGO T-A7670G | Supported | LTE, GPS shield, MQTT over LTE |

## Quick start

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

See `docs/index.md` for the full documentation.
