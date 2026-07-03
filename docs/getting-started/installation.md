# Installation

## Requirements
- ESP32-WROOM development board
- CAN transceiver
- PlatformIO
- MQTT broker
- Web server for the dashboard

## Firmware
```bash
cd firmware/esp32-wroom
pio run
pio run -t upload
```

## First configuration
After the first boot, connect to the MOT setup access point and configure WiFi, MQTT and OTA.

## Dashboard
Upload the `dashboard/` folder to your web server and edit `dashboard/config.js`.

## Validation
Check:
- MQTT connected
- CAN values are published
- Dashboard shows live data
- OTA upload works
