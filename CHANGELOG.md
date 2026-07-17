
## Unreleased

- Integrate shared GPS parsing into ESP32-WROOM production firmware.
- Publish location to MQTT/AWS and ABRP only for a current valid fix.
- Preserve retained coordinates and label stale coordinates as the last known location in the dashboard.
# Changelog

## v1.1.0-lilygo-stability

### Added
- LilyGO T-A7670G firmware path.
- LewisXhe TinyGSM A76XXSSL transport for A7670G-LLSE.
- LTE modem initialization and GPRS/PDP connection.
- MQTT over LTE.
- External L76K GPS support.
- CAN receive diagnostics.
- Backup/Restore for JSON configuration.
- Factory Reset.
- Modem recovery after OTA/reset.

### Known issues
- ABRP over LTE HTTPS is disabled/deferred.
- Device online heartbeat is planned but not yet implemented.
