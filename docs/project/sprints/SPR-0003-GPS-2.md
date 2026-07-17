# SPR-0003 – GPS-2 Production Integration

## Status

Implemented; hardware and deployed-cloud validation pending.

## Goal

Use the shared `MotGps` library in both production firmware families and expose only current, valid fixes to MQTT/AWS and ABRP. Retained cloud coordinates remain untouched when reception is unavailable, allowing the dashboard to display the last known location.

## Architecture

- ESP32-WROOM and LilyGO continuously parse GNSS data through `MotGps`.
- Firmware publishes `location/*` only while `MotGps::valid()` is true.
- Firmware never clears retained latitude/longitude on fix loss.
- ABRP receives `lat`/`lon` only while the local fix is valid.
- DynamoDB `metadata[topic].receivedAt` is the source of truth for location age.
- Dashboard labels a coordinate pair as `Aktueller Standort` for the configured freshness window and afterward as `Letzter Standort`.

## Changed components

- `firmware/shared-libs/MotGps`
- `firmware/esp32-wroom`
- `firmware/lilygo-t-a7670` (validated existing guards)
- `firmware/common/abrp`
- `dashboard/js/app.js`
- `dashboard/config*.js`

## Validation

- Build `esp32dev`, `esp32dev-aws`, and `esp32dev-gps-test`.
- Build LilyGO production environment.
- Verify no location publish before first fix.
- Verify retained coordinates remain after fix loss.
- Verify ABRP payload omits `lat` and `lon` without a valid fix.
- Verify dashboard changes from current to last-known after `locationCurrentSeconds`.

## ADR

No new ADR required. The implementation follows the existing current-state-store and REST snapshot architecture.
