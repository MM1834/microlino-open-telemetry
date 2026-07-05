# Telemetry Data Flow

```text
Microlino Display CAN
  -> ESP32 TWAI
  -> CAN frame ring buffer
  -> decoder engine
  -> shared telemetry model
  -> JSON API
  -> Web UI
  -> MQTT
  -> ABRP
```

## Raw CAN

Raw CAN frames are exposed for debugging.

LilyGO endpoints:

```text
GET /api/lilygo/can
GET /api/lilygo/can/frames
```

## Decoded telemetry

Decoded telemetry is exposed as JSON.

```text
GET /api/telemetry
```

## System status

```text
GET /api/status
```
