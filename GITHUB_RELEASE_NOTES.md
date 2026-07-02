# Microlino Open Telemetry v1.0.0

First usable release of Microlino Open Telemetry (MOT).

> One telemetry model. Multiple vehicles. Open platform.

## Highlights

- ESP32-WROOM firmware baseline
- Microlino Display-CAN telemetry
- MQTT publishing
- Configurable vehicle ID and MQTT prefix
- Web configuration UI
- JSON status endpoint
- MOT Dashboard v1.0 cockpit UI
- WSS-ready dashboard connection
- Microlino SVG illustration

## Default MQTT structure

```text
mot/pioneer/display/soc
mot/pioneer/display/speed_kmh
mot/pioneer/display/odometer_km
mot/pioneer/charging/is_charging
```

## Recommended dashboard setup

```js
host: "mmds.muehlberg.ch",
port: 20226,
useTls: true,
topicPrefix: "mot",
vehicleId: "pioneer"
```

## Notes

Replace `dashboard/libs/mqtt.min.js` with the working MQTT.js browser build before uploading the dashboard.
