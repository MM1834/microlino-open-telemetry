# Sprint 3.5.1 WROOM RC2

This package fixes the Sprint 3.5 firmware refactor boundary.

Principle:

- `firmware/common/` contains platform-independent logic only.
- `firmware/esp32-wroom/` owns board-specific setup: WiFi, MQTT client, Web UI, CAN/TWAI.
- Common decoders write into `MotTelemetry`.
- Board outputs publish/read `MotTelemetry`.

This package is intended to replace the previous Sprint 3.5 files on the `develop` branch.

Test:

```bash
cd firmware/esp32-wroom
pio run
pio run -t upload
pio device monitor
```
