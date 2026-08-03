# Changelog v1.0.2 Firmware Polish

## Added
- Stable configurable device name.
- Stable MQTT client ID derived from device name.
- MQTT optional mode: no host means MQTT disabled.
- ABRP optional mode with minimal telemetry sender.
- Configuration export as JSON.
- Configuration import from JSON.
- Documentation for optional services and configuration management.

## Changed
- MQTT no longer attempts to connect when no host is configured.
- MQTT publishes device name and MQTT client ID under system topics.
