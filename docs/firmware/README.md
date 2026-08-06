# Firmware Documentation

> **Status:** Current index; runtime remains unverified
>
> **Audience:** Firmware developer and maintainer

## Current source-based reference

- [Overview and environments](overview.md)
- [Architecture](architecture.md)
- [CAN and decoder pipeline](can.md)
- [Pioneer Standard-CAN decoder and charge evidence](pioneer-standard-can.md)
- [CAN signal matrix — Display CAN, Pioneer Standard CAN and V2](can-signal-matrix.rtf)
- [GPS](gps.md)
- [Network](network.md)
- [LTE/GPRS](lte.md)
- [Configuration](configuration.md)
- [MQTT](mqtt.md)
- [OTA](ota.md)
- [Known gaps](known-gaps.md)
- [ESP32-C6 qualification firmware](../../firmware/esp32-c6/README.md)
- [Local HTTP API](../api/local-device-api.md)
- [MQTT topic contract](../api/mqtt-topics.md)

## Historical firmware notes

Files named after a release, fix, debug/trace experiment, transport migration or
stability increment are retained as engineering history. This includes most
`lilygo-*fix`, `*debug`, `*trace`, `*stack-v*`, `*transport` and version-specific
pages. Those records are available from Git history when an investigation requires
them.

Historical detail may explain current code but does not establish current build or
hardware validation.
