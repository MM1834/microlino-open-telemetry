# Microlino Open Telemetry (MOT)

> **One telemetry model. Multiple vehicles. Open platform.**

Microlino Open Telemetry (MOT) is an open-source telemetry platform for the Microlino EV.

MOT reads vehicle data from the Microlino Display CAN bus, decodes it on an ESP32, publishes live telemetry through MQTT, and displays it in a browser-based dashboard.

The project is designed around one common telemetry model so that firmware, MQTT, dashboards, OTA updates, ABRP integration and future LilyGO/LTE hardware can evolve without changing the data structure.

---

## Current status

**Release candidate:** `v1.0.0-rc1`

Current tested setup:

- ESP32-WROOM DevKit
- SN65HVD230 / VP230 CAN transceiver
- Microlino Display CAN via OBD2
- WiFi + MQTT
- MQTT over WebSocket/WSS for the dashboard
- ESP32 Web configuration
- OTA firmware upload via Web UI
- Cockpit-style web dashboard

---

## Features

### Firmware

- ESP32-WROOM support
- Microlino Display CAN decoder
- SOC, speed, odometer and charging-state telemetry
- Configurable MQTT broker
- Configurable MQTT prefix and vehicle ID
- Fallback WiFi access point
- Web configuration interface
- JSON status API
- OTA firmware update with password protection

### Dashboard

- Browser-based dashboard
- Dark cockpit UI
- Microlino illustration
- MQTT over WebSocket / WSS
- SOC, range, speed, odometer and charging status
- Online/offline status
- Prepared location panel
- Responsive layout for desktop and mobile

---

## MQTT topic structure

MOT uses the following topic pattern:

```text
mot/<vehicleId>/<category>/<value>
```

Example:

```text
mot/pioneer/display/soc
mot/pioneer/display/speed_kmh
mot/pioneer/display/odometer_km
mot/pioneer/charging/is_charging
mot/pioneer/system/device_id
```

See [docs/MQTT.md](docs/MQTT.md).

---

## Architecture

```text
                 Microlino
                     │
                Display CAN
                     │
                     ▼
              ESP32-WROOM
                     │
              CAN Decoder
                     │
                     ▼
              Telemetry Model
                     │
      ┌──────────────┼──────────────┐
      │              │              │
    MQTT         JSON API          OTA
      │
      ▼
  Dashboard / ioBroker / Home Assistant / Node-RED
```

---

## Quick start

### 1. Flash firmware

```bash
cd firmware/esp32-wroom
pio run -t upload
pio device monitor
```

### 2. Configure ESP32

Open the ESP32 Web UI and configure:

```text
WiFi SSID
WiFi password
MQTT host
MQTT port
MQTT username/password
MQTT prefix: mot
Vehicle ID: pioneer
Vehicle name: Microlino Pioneer
OTA password
```

### 3. Configure dashboard

Copy and adapt the dashboard configuration:

```bash
cp dashboard/config.example.js dashboard/config.js
```

Example:

```js
window.MOT_CONFIG = {
  mqtt: {
    host: "mqtt.example.com",
    port: 443,
    useTls: true,
    path: "/",
    username: "",
    password: "",
    topicPrefix: "mot",
    vehicleId: "pioneer"
  }
};
```

### 4. Deploy dashboard

Upload the `dashboard/` folder to any static web host.

For HTTPS-hosted dashboards, MQTT WebSocket must be available as `wss://`, not plain `ws://`.

See [docs/DASHBOARD.md](docs/DASHBOARD.md) and [docs/SECURE_WEBSOCKET_CADDY.md](docs/SECURE_WEBSOCKET_CADDY.md).

---

## Roadmap

### v1.0.0

- Stable ESP32-WROOM release
- Display CAN telemetry
- MQTT dashboard
- OTA updates
- Documentation

### v1.1

- Dashboard refinements
- Historical charts
- More system telemetry
- Config backup/restore
- ABRP integration

### v2.0

- LilyGO LTE/GPS support
- Cellular connectivity
- GPS location
- Optional second CAN / BMS data

---

## Contributing

Contributions, testing results and CAN decoding notes are welcome.

Please see [CONTRIBUTING.md](CONTRIBUTING.md).

---

## License

Released under the MIT License.

Microlino® is a registered trademark of Micro Mobility Systems AG.

Microlino Open Telemetry is an independent open-source community project and is not affiliated with or endorsed by Micro Mobility Systems AG.
