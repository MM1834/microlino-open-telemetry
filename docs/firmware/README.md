# Firmware Documentation

> **Status:** Current index; runtime remains unverified
>
> **Audience:** Firmware developer and maintainer

## Current source-based reference

- [Overview and environments](overview.md)
- [ESP32-C6 release notes and XIAO feature boundary](c6-release-notes.md)
- [Architecture](architecture.md)
- [CAN and decoder pipeline](can.md)
- [Pioneer Standard-CAN decoder and charge evidence](pioneer-standard-can.md)
- [CAN signal matrix — Display port, Standard-CAN V1/V2 and WiCAN Lite](can-signal-matrix.md)
- [GPS](gps.md)
- [Network](network.md)
- [LTE/GPRS](lte.md)
- [Configuration](configuration.md)
- [Backup, restore and factory reset](backup-restore.md)
- [MQTT](mqtt.md)
- [OTA](ota.md)
- [Known gaps](known-gaps.md)
- [ESP32-C6 qualification firmware](../../firmware/esp32-c6/README.md)
- [MOT Lite for original WiCAN](../../firmware/wican-lite/README.md)
- [MOT Display firmware](../../firmware/mot-display-c6/README.md)
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
