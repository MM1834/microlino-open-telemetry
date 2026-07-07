# ABRP Configuration

## Fields

- ABRP enabled
- ABRP API key
- ABRP user token

## Behavior

ABRP telemetry is sent periodically when enabled and configured.

Latitude and longitude are only included when a valid GPS fix exists.

## LilyGO status

```text
GET  /api/lilygo/abrp
POST /api/lilygo/abrp/test
```

The status includes:

- enabled
- configured
- transport
- timeValid
- lastSuccess
- HTTP code
- ABRP response message
- last payload
