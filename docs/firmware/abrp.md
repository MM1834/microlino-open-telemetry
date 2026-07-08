# ABRP

ABRP integration builds telemetry payloads for A Better Routeplanner.

## Current status

- ABRP over WiFi is retained.
- ABRP over LTE HTTPS is deferred.

## Planned work

- Isolated `TinyGsmClientSecure` test
- HTTPS request to `api.iternio.com`
- Timeout/backoff logic
- Non-blocking behavior so WebUI is not impacted
