## Local Dashboard History

MOT v1.0.1 adds a local browser-based History Engine.

The dashboard stores selected telemetry samples in IndexedDB and visualizes:

- SoC over time
- Speed over time

The feature is designed for demonstrations and beta testing. It does not require ioBroker, InfluxDB or any backend database.

History controls include:

- 24h / 7d / 30d range selection
- Demo data generator
- CSV export
- JSON export
- Clear local history

See [`docs/dashboard/history.md`](docs/dashboard/history.md).
