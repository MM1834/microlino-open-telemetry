# GPS-2 – Production integration

GPS-2 promotes the shared `MotGps` implementation from isolated hardware testing into both production firmware families.

## Publication rule

Latitude, longitude, speed, satellite count, HDOP and fix age are published only when the local GPS fix is valid and no older than the configured maximum age. When reception is lost, firmware sends no replacement coordinates and does not clear retained MQTT values.

This is intentional for EV use in underground garages and covered charging locations: the cloud continues to retain the last known position.

## Dashboard interpretation

The REST snapshot already exposes a `receivedAt` timestamp for every topic in `metadata`. The dashboard uses the older of latitude and longitude timestamps for the coordinate pair:

- within `dashboard.locationCurrentSeconds`: **Aktueller Standort**
- afterward: **Letzter Standort**

The global snapshot `updatedAt` is not used for this decision because unrelated telemetry may continue to update.

## ABRP

ABRP receives `lat` and `lon` only when the firmware has a current valid fix. Cached cloud coordinates are never used for ABRP.
