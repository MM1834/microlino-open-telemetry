# Dashboard History

MOT v1.0.1 introduces a local dashboard history engine.

## Purpose

The history feature is intended for:

- demonstrations,
- beta testing,
- short-term diagnostics,
- checking whether telemetry values change over time.

It does not require ioBroker, InfluxDB, Home Assistant or any backend database.

## Storage

History is stored in the browser using IndexedDB.

```text
Database: mot-history
Store:    samples
Key:      <vehicleId>:<timestamp>
```

This means:

- history is stored per browser/device,
- clearing browser data removes the history,
- another phone or computer has its own separate history.

## Recorded values

The current implementation stores:

| Field | Unit | Source topic |
|---|---:|---|
| SoC | % | `display/soc` |
| Speed | km/h | `display/speed_kmh` |
| Range | km | `display/estimated_range_km` |
| Charging state | boolean | `charging/is_charging` |
| Power display | dashboard value | `charging/power_display` |
| RSSI | dBm | `system/rssi` |

## Sampling

The dashboard stores a sample when:

- at least 60 seconds have passed since the last stored sample, or
- SoC changes, or
- charging state changes.

The default retention is 30 days.

## Charts

The dashboard currently displays:

- SoC Verlauf
- Speed Verlauf

Available ranges:

- 24h
- 7d
- 30d

## Demo data

For presentations and local testing, use the **Demo-Daten** button.

This creates a synthetic 24-hour drive profile with SoC and speed values.

## Export

The dashboard can export the local history as:

- CSV
- JSON

## Clear history

The **History löschen** button removes the local history for the configured vehicle.
