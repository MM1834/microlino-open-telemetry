> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO TinyGSM LTE Client

Replaces the custom AT socket implementation used by MQTT with a TinyGSM-backed Arduino `Client`.

## Why

The custom AT layer could open a socket and write MQTT CONNECT, but receive handling stayed unreliable (`available()` returned 0). The reference obd2mqtt code uses TinyGSM instead of a custom `AT+CIPOPEN` / `AT+CIPSEND` / `AT+CIPRXGET` implementation.

## Scope

This patch only replaces:

```text
src/lte/lilygo_lte_client.cpp
```

The rest of the firmware remains unchanged:

- Web UI
- Config
- OTA
- CAN
- GPS
- MQTT high-level logic
- ABRP WiFi path
- Modem status API

## Trace

```text
GET  /api/lilygo/lte/mqtt-trace
POST /api/lilygo/lte/mqtt-trace/clear
```

The trace now reports `backend: TinyGSM`.
