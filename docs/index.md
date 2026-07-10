# Microlino Open Telemetry documentation

Microlino Open Telemetry is an ESP32-based telemetry platform for Microlino vehicles. It combines CAN decoding, MQTT, a local WebUI, GPS and an experimental LilyGO LTE path.

## Start here

| Section | Description |
|---|---|
| [Getting started](getting-started/overview.md) | Select hardware, flash firmware and complete first setup |
| [Hardware guide](hardware/overview.md) | ESP32-WROOM, WeAct CAN485, LilyGO, GPS and enclosure |
| [WebUI guide](webui/overview.md) | Configure and diagnose the local device WebUI |
| [Dashboard guide](dashboard/overview.md) | Use the browser/mobile telemetry dashboard |
| [Firmware guide](firmware/overview.md) | Architecture, MQTT, CAN, GPS, OTA and backup |
| [API reference](api/rest-api.md) | REST endpoints, MQTT topics and backup JSON |
| [Troubleshooting](troubleshooting/overview.md) | Common setup and runtime problems |
| [Developer documentation](developer/README.md) | Implementation notes and experimental transport work |
| [Roadmap](roadmap/index.md) | Planned product and security work |
| [Releases](releases/v1.1.0-lilygo-stability.md) | Release-specific notes |

## Current platform status

| Platform/function | Status |
|---|---:|
| ESP32-WROOM WiFi/CAN | Stable |
| WeAct Studio ESP32 CAN485 | Compatible |
| LilyGO WiFi/GPS/CAN | Working |
| LilyGO LTE registration/GPRS | Working |
| MQTT over LilyGO LTE | Experimental |
| ABRP over LTE HTTPS | Deferred |
| AP/WebUI security hardening | Planned |
| Secure MQTT / AWS IoT evaluation | Planned |

## Typical journeys

### New user

1. [Choose hardware](hardware/overview.md).
2. [Install firmware](getting-started/installation.md).
3. [Complete first start](getting-started/first-start.md).
4. [Configure the WebUI](webui/overview.md).
5. [Create a backup](webui/backup-restore.md).

### Developer

1. Read the [firmware architecture](firmware/architecture.md).
2. Review the [MQTT model](firmware/mqtt.md).
3. Review [CAN decoding](firmware/can.md).
4. Use the [developer archive](developer/README.md) for implementation history.
