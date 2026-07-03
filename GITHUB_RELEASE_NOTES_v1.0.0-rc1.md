# Microlino Open Telemetry v1.0.0-rc1

First release candidate for MOT v1.0.

## Highlights

- ESP32-WROOM firmware for Microlino Display CAN telemetry
- MQTT publishing with topic structure `mot/<vehicleId>/...`
- Configurable MQTT prefix, vehicle ID and vehicle name
- Web configuration UI on the ESP32
- Public dashboard with MQTT over WebSocket/WSS support
- Cockpit-style dashboard with Microlino illustration
- SOC, range, speed, odometer, charging and system status
- Prepared project documentation and release workflow

## Notes

This is a release candidate. Use it for testing and feedback before the final v1.0.0 release.

Private MQTT hosts, credentials and API keys are intentionally not included. Copy `dashboard/config.example.js` to `dashboard/config.js` and adapt it for your own deployment.
