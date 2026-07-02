# Microlino Open Telemetry (MOT)

> **One telemetry model. Multiple vehicles. Open platform.**

Microlino Open Telemetry (MOT) is an open-source telemetry platform for the Microlino EV.

MOT reads vehicle data from one or more CAN buses, decodes it into a unified telemetry model, and makes this information available through MQTT, JSON APIs, dashboards, and future cloud services.

The project is designed to support multiple Microlino generations while remaining hardware independent and easy to extend.

---

## Features

### Current

- ✅ ESP32-WROOM support
- ✅ Microlino Display CAN decoder
- ✅ MQTT telemetry
- ✅ Embedded web configuration
- ✅ JSON status API
- ✅ WiFi fallback access point
- ✅ Configuration stored in NVS

### Planned

- 🚧 Progressive Web Dashboard (PWA)
- 🚧 ABRP integration
- 🚧 OTA firmware updates
- 🚧 LilyGO LTE/GPS support
- 🚧 Dual CAN support
- 🚧 Pioneer BMS decoder
- 🚧 Standard BMS decoder

---

## Supported Hardware

### Current

- ESP32-WROOM DevKit
- SN65HVD230 / VP230 CAN transceiver
- OBD-II connection

### Planned

- LilyGO T-A7670
- Dual CAN modules
- LTE
- GPS

---

# System Architecture

```text
                 Microlino

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
      │              │
 Dashboard      Third-party Apps
```

The decoder layer is completely independent from all outputs.

Every decoder writes only into the shared telemetry model.

MQTT, JSON APIs, dashboards and cloud integrations simply consume the telemetry model.

This architecture allows support for multiple Microlino generations while keeping all integrations identical.

---

# Repository Structure

```text
microlino-open-telemetry/

firmware/
    common/
    esp32-wroom/
    lilygo-t-a7670/

dashboard/
    public/
        css/
        js/
        icons/

docs/
    can/

hardware/
    esp32-wroom/
    lilygo/
    sn65hvd230/
    obd2/

tools/
```

---

# Roadmap

| Version | Description |
|----------|-------------|
| **v0.9.0** | Project foundation |
| **v0.9.1** | Dashboard prototype |
| **v0.9.2** | ABRP integration |
| **v0.9.3** | OTA support |
| **v1.0.0** | First stable ESP32-WROOM release |
| **v2.0.0** | LilyGO LTE/GPS support |
| **v2.1.0** | Dual CAN support |

---

# Project Philosophy

Microlino Open Telemetry is built around one simple principle:

> **One telemetry model. Multiple vehicles. Open platform.**

Rather than creating separate firmware versions for different Microlino generations, MOT provides a single telemetry model shared by all supported vehicles.

Vehicle-specific CAN decoders translate raw CAN frames into the common telemetry model.

This allows MQTT, JSON APIs, dashboards, ABRP and future integrations to work without knowing which Microlino model produced the data.

---

# Why MOT?

The goal of this project is not only to build another ESP32 CAN interface.

The goal is to provide an open, documented and extensible telemetry platform for the Microlino community.

Future versions will support:

- Multiple Microlino generations
- Multiple CAN buses
- Cloud connectivity
- Local dashboards
- Home Assistant
- Node-RED
- ABRP
- Mobile applications

while keeping one common telemetry model.

---

# Contributing

Contributions are very welcome.

Whether you are improving CAN decoders, testing new hardware, documenting reverse engineering results or improving the dashboard, every contribution helps the project.

Contribution guidelines will be published in `CONTRIBUTING.md`.

---

# License

Released under the MIT License.

Microlino® is a registered trademark of Micro Mobility Systems AG.

Microlino Open Telemetry is an independent open-source community project and is not affiliated with or endorsed by Micro Mobility Systems AG.
