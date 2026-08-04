# Firmware Documentation

> **Status:** Current index; runtime remains unverified
>
> **Audience:** Firmware developer and maintainer

## Current source-based reference

- [Overview and environments](overview.md)
- [Architecture](architecture.md)
- [CAN and decoder pipeline](can.md)
- [GPS](gps.md)
- [Network](network.md)
- [LTE/GPRS](lte.md)
- [Configuration](configuration.md)
- [MQTT](mqtt.md)
- [OTA](ota.md)
- [Known gaps](known-gaps.md)
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
