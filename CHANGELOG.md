# Changelog

## v1.0.0 - First usable MOT release

### Added

- ESP32-WROOM firmware baseline for Microlino Open Telemetry.
- CAN Display decoder baseline.
- MQTT telemetry publishing.
- Configurable MQTT prefix, vehicle ID and vehicle name.
- Embedded firmware Web UI configuration.
- JSON status endpoint.
- Fallback WiFi AP.
- MOT Dashboard v1.0 cockpit layout.
- Microlino-style SVG illustration.
- MQTT over WebSocket dashboard support.
- WSS-ready dashboard configuration.
- Location panel placeholder for GPS/location support.

### Changed

- MQTT topic structure aligned to:

```text
mot/<vehicleId>/<domain>/<field>
```

Example:

```text
mot/pioneer/display/soc
mot/pioneer/charging/is_charging
```

### Notes

- The dashboard requires a browser MQTT.js build at `dashboard/libs/mqtt.min.js`.
- For HTTPS-hosted dashboard pages, MQTT WebSocket access must use WSS, not plain WS.
- ESP32 can continue using plain MQTT inside the local network.
