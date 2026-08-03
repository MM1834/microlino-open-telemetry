# Microlino Open Telemetry v1.0.0-rc1

First release candidate for MOT v1.0.

## Highlights

- ESP32-WROOM firmware for Microlino Display CAN telemetry
- MQTT topic structure `mot/<vehicleId>/...`
- Web configuration UI
- OTA firmware update via browser upload
- OTA password protection
- Cockpit-style dashboard
- MQTT over WebSocket/WSS dashboard support
- Tested with real Microlino OBD2 Display CAN data

## Tested core telemetry

- SOC
- Speed
- Odometer
- Charging indicator candidate
- MQTT publishing
- Dashboard connection over WSS
- OTA update flow

## Important notes

This is a **release candidate**.

Use it for beta testing before the final v1.0.0 release.

Do not expose plain MQTT or plain WebSocket directly to the internet. For browser dashboards served over HTTPS, use WSS through a reverse proxy such as Caddy.

## Default topic structure

```text
mot/pioneer/display/soc
mot/pioneer/display/speed_kmh
mot/pioneer/display/odometer_km
mot/pioneer/charging/is_charging
mot/pioneer/system/device_id
```

## Configuration

Private MQTT hosts, credentials and API keys are intentionally not included.

Copy:

```bash
cp dashboard/config.example.js dashboard/config.js
```

Then adapt `dashboard/config.js` for your broker.
