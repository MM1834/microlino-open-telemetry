# Microlino Open Telemetry (MOT)

> One telemetry model. Multiple vehicles. Open platform.

🚗 Open-source telemetry platform for the Microlino EV.

Microlino Open Telemetry (MOT) is an open-source platform that provides
real-time telemetry for the Microlino using ESP32-based hardware.

Designed around a single telemetry model, MOT supports multiple
Microlino generations while remaining hardware independent.

## Features

✔ Display CAN support
✔ MQTT
✔ Embedded Web Configuration
✔ JSON API
✔ Progressive Web Dashboard (planned)
✔ ABRP Integration (planned)
✔ OTA Updates (planned)
✔ LTE/GPS (LilyGO) (planned)

## Supported Hardware

- ESP32-WROOM
- LilyGO T-A7670
- SN65HVD230 CAN Transceiver

## Architecture

                Microlino
                     │
        ┌────────────┴────────────┐
        │                         │
   Display CAN               BMS CAN
        │                         │
        └────────────┬────────────┘
                     │
              Decoder Engine
                     │
                     ▼
             Telemetry Model
                     │
      ┌──────────────┼──────────────┐
      │              │              │
    MQTT         JSON API        ABRP
      │
 Dashboard / Mobile Apps

## Roadmap

v0.9  Foundation
v0.9.1 Dashboard
v0.9.2 ABRP
v0.9.3 OTA
v1.0   First Stable Release
v2.0   LilyGO LTE/GPS
v2.1   Dual CAN Support

## Project Philosophy

One telemetry model.
Multiple vehicles.
Open platform.
