# Changelog

## v1.0.2

### Added
- Firmware-side MQTT diagnostics helper.
- `/api/mqtt-test` endpoint.
- System Health helper.
- `/api/system-health` endpoint.
- WebConfig UI snippet for System Health.
- Documentation for MQTT diagnostics and System Health.

### Notes
- `rc=-2` is now explained as TCP connection failure.
- Firmware still uses plain MQTT over TCP, not WebSocket/WSS.
