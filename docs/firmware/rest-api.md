# REST API overview

The exact endpoint set differs between ESP32-WROOM and LilyGO builds. Verify route names against the current WebUI source before relying on them in external automation.

## Commonly documented endpoints

```text
GET  /api/status
GET  /api/telemetry
GET  /api/mqtt-test
GET  /api/system-health
```

## LilyGO status and diagnostics

Historic/current documentation references:

```text
GET  /api/lilygo/mqtt
GET  /api/lilygo/mqtt/debug
GET  /api/lilygo/can
GET  /api/lilygo/can/frames
GET  /api/lilygo/abrp
POST /api/lilygo/abrp/test
GET  /api/lilygo/lte/debug
GET  /api/lilygo/lte/tcp-test
GET  /api/lilygo/lte/rx-debug
GET  /api/lilygo/lte/mqtt-trace
POST /api/lilygo/lte/mqtt-trace/clear
```

Some LTE endpoints were created for experimental stacks and may not exist in the latest cleaned transport. Treat them as developer diagnostics rather than stable public API.

## Configuration operations

The WebUI also provides export/import, OTA and factory-reset operations. These routes should be authenticated before an end-user release.
