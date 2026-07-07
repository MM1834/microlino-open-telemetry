# LilyGO ABRP WiFi

Adds ABRP telemetry send for LilyGO.

Transport:

```text
WiFi only in this patch
```

Payload fields:

- `soc`
- `utc` when system time is valid
- `speed`
- `power`
- `is_charging`
- `lat` / `lon` only when L76K GPS fix is valid

Endpoints:

```text
GET  /api/lilygo/abrp
POST /api/lilygo/abrp/test
```

LTE ABRP HTTP transport follows after LTE TCP/MQTT transport is confirmed stable.
