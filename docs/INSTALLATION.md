# Installation Guide

## Firmware

Requirements:

- PlatformIO
- ESP32-WROOM DevKit
- SN65HVD230 / VP230 CAN transceiver

Build and upload:

```bash
cd firmware/esp32-wroom
pio run -t upload
pio device monitor
```

## Initial configuration

After boot, connect to the ESP32 Web UI or fallback AP and configure:

- WiFi SSID
- WiFi password
- MQTT host
- MQTT port
- MQTT username/password
- MQTT prefix: `mot`
- Vehicle ID: `pioneer`
- Vehicle name: `Microlino Pioneer`
- OTA password

## Dashboard

Copy configuration template:

```bash
cp dashboard/config.example.js dashboard/config.js
```

Edit `dashboard/config.js` for your broker.

Upload the `dashboard/` directory to your web server.

## HTTPS deployments

If the dashboard uses HTTPS, configure MQTT WebSocket as WSS.

See `docs/SECURE_WEBSOCKET_CADDY.md`.
