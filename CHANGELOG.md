# Changelog

## v1.0.0-rc1

First release candidate for Microlino Open Telemetry.

### Added

- ESP32-WROOM firmware for Microlino Display CAN telemetry
- SOC decoding
- Speed decoding
- Odometer decoding
- Charging-state detection
- MQTT publisher
- MQTT topic structure `mot/<vehicleId>/...`
- Configurable MQTT prefix
- Configurable vehicle ID
- Configurable vehicle display name
- ESP32 Web configuration UI
- JSON status API
- WiFi fallback AP
- OTA firmware update via Web UI
- OTA password protection
- Cockpit-style dashboard
- MQTT over WebSocket/WSS dashboard support
- Caddy reverse-proxy documentation for secure WSS
- Release documentation

### Changed

- MQTT topics moved from legacy flat/prefix topics to structured topics:

```text
microlino/display/soc
```

to:

```text
mot/<vehicleId>/display/soc
```

### Notes

This is a release candidate intended for beta testing before the final v1.0.0 release.
