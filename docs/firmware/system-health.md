# System health and diagnostics

The firmware exposes status information for runtime and connectivity diagnostics.

## Core health data

- device ID and firmware version,
- uptime,
- IP address and network mode,
- WiFi RSSI,
- MQTT state,
- CAN validity and counters,
- GPS freshness,
- LilyGO modem/GPRS state.

## Layered diagnosis

Diagnose communications in this order:

```text
network interface
-> DNS
-> TCP
-> MQTT authentication/session
-> publish/receive
```

A successful TCP connection does not prove a complete MQTT session. MQTT additionally requires a valid broker `CONNACK`.

## Useful evidence for bug reports

Include:

- serial boot/runtime log,
- `/api/status` JSON,
- MQTT status/debug JSON,
- broker log timestamps,
- active transport,
- firmware version and board.
