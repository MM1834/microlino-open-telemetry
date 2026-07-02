# MOT Sprint 2 — Telemetry Core

This package adds the first shared telemetry model and a dashboard prototype.

It is intentionally platform-independent. Firmware targets such as ESP32-WROOM and LilyGO should use the shared telemetry model instead of defining their own output variables.

## Contents

```text
firmware/common/telemetry/
  telemetry.h
  telemetry.cpp

firmware/common/mqtt/
  mqtt_topics.h
  mqtt_topics.cpp

firmware/common/api/
  telemetry_json.h
  telemetry_json.cpp

dashboard/public/
  index.html
  css/style.css
  js/app.js
  manifest.json

docs/
  TELEMETRY_MODEL.md
```

## Integration idea

Firmware decoders write into `TelemetryState telemetry`.

Outputs read from it:

- MQTT publisher
- JSON API
- ABRP upload
- embedded web UI
- external dashboard

## Next step

Wire the existing ESP32-WROOM firmware to this telemetry model and update its MQTT output to use the topic helpers.
