# LTE development

> **Status:** Historical investigation collection; current LTE beta readiness is unverified.

The LilyGO LTE path went through several implementations:

1. custom SIMCom AT socket functions,
2. URC-buffer receive handling,
3. `CIPRXGET` experiments,
4. LewisXhe TinyGSM A76XX transport,
5. direct/native client and task/concurrency experiments.

The current LilyGO source still depends on the LewisXhe TinyGSM repository, but
DOC-001 has not rebuilt or hardware-tested the path. AWS IoT firmware currently
uses WiFi only; LTE transport code is confined to the legacy MQTT branch and
diagnostic/network components.

## Important findings

- The tested A7670G-LLSE firmware returned `ERROR` for `AT+CIPSTATUS`.
- `AT+CIPRXGET` could report `operation not supported`.
- Plain `OK` after `AT+CIPOPEN` is not necessarily the final socket result.
- Broker logs proved that MQTT `CONNECT` reached the broker even when firmware timed out on `CONNACK`.
- Concurrent GPRS reconnect and MQTT socket activity can corrupt modem/client state.
- Blocking MQTT tasks can trigger the ESP32 task watchdog.
- Power and USB/battery quality remain a separate hardware variable to test.

The files in this directory preserve the detailed investigation trail.
