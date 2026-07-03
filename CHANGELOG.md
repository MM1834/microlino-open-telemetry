# Changelog

## v1.0.1

### Added
- Dashboard History Engine using browser IndexedDB.
- SoC history chart.
- Speed history chart.
- 24h / 7d / 30d history range selector.
- Demo data generator.
- CSV and JSON export for local history.
- Clear local history action.
- Debug summary for history samples.

### Changed
- Dashboard now records telemetry samples locally after MQTT updates.
- Demo data generation includes both SoC and speed profiles.

### Fixed
- History recorder now supports the dashboard's flat MQTT topic value map.

### Notes
- No firmware changes are required.
- History is stored locally in the browser and is not synchronized across devices.

## v1.0.0

### Added
- Initial stable ESP32-WROOM firmware.
- MQTT telemetry.
- Responsive dashboard.
- OTA update support.
- Documentation and screenshots.
