# Getting Started

This guide describes the current development setup for Microlino Open Telemetry.

## Current Status

MOT is in early development.

The current working prototype targets:

- ESP32-WROOM DevKit
- SN65HVD230 / VP230 CAN transceiver
- Microlino Display CAN
- WiFi
- MQTT

## Requirements

- ESP32-WROOM DevKit
- SN65HVD230 / VP230 CAN transceiver
- PlatformIO
- MQTT broker
- Microlino Display CAN access

## Reference Wiring

| ESP32 | CAN Module |
|---|---|
| GPIO26 | CTX / TXD |
| GPIO27 | CRX / RXD |
| 3V3 | 3V3 |
| GND | GND |

| CAN Module | Vehicle |
|---|---|
| CANH | Display CAN-H |
| CANL | Display CAN-L |

## Flashing Firmware

The stable firmware layout is still being prepared.

Current development uses PlatformIO:

```bash
pio run -t upload
pio device monitor
```

## First Boot

On first boot or if WiFi cannot connect, the firmware should start a fallback access point.

Expected fallback AP:

```text
MOT-XXXXXX
```

Open:

```text
http://192.168.4.1/config
```

Configure:

- WiFi SSID
- WiFi password
- MQTT host
- MQTT port
- MQTT username/password if needed
- vehicle name
- MQTT prefix

## MQTT

After configuration, telemetry is published to MQTT.

Example topics:

```text
mot/<vehicle-id>/display/soc
mot/<vehicle-id>/display/odo
mot/<vehicle-id>/charging/is_charging
```

See `docs/MQTT.md` for the topic specification.

## JSON API

The embedded firmware exposes a JSON status API:

```text
/api/status
```

See `docs/API.md`.

## Safety Notes

The firmware currently uses CAN listen-only mode for the ESP32 TWAI controller.

Always verify wiring before connecting to the vehicle.

Use a proper 12 V to 5 V step-down converter when powering from OBD-II.
