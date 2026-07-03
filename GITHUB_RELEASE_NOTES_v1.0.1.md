# MOT v1.0.1

## Highlights

MOT v1.0.1 is a dashboard-focused maintenance release. It adds a local browser-based History Engine for beta testing and demonstrations without requiring ioBroker, InfluxDB, Home Assistant or any server-side database.

## Added

- Local browser history using IndexedDB
- SoC history chart
- Speed history chart
- 24h / 7d / 30d range selector
- Demo data generator for presentations and testing
- CSV export
- JSON export
- Clear local history button
- Small dashboard debug summary for stored samples

## Changed

- Dashboard now records selected telemetry values locally after MQTT updates.
- Demo data now includes realistic SoC and speed profiles.

## Notes

History data is stored locally in the browser. It is device/browser-specific and can be cleared by the user.

No firmware update is required for v1.0.1 if you already run v1.0.0.
