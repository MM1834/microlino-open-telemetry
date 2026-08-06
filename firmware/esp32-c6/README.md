# ESP32-C6 firmware

This is the shared C6 firmware line established by C6-001 for the Muse Lab nanoESP32-C6-N16 and
Seeed XIAO ESP32-C6 board profiles.

Current implemented slice:

- board-specific PlatformIO environments and flash partitions;
- two concurrent ESP32-C6 TWAI controllers at 500 kbit/s;
- listen-only operation with independent counters and decoder profiles;
- persistent independent CAN1/CAN2 profile selection in Preferences/NVS;
- Display-CAN, Standard-CAN V1 - Pioneer and Standard-CAN V2 shared decoders;
- board, flash, CAN and GPS pin diagnostics;
- shared optional-GPS detection and NMEA fix-state handling;
- bounded in-memory drive capture with on-demand summaries for known CAN IDs;
- WiFi station configuration and optional per-device AWS IoT publication;
- XIAO external-antenna selection.

Build without flashing:

```sh
pio run -e nanoesp32c6-n16
pio run -e xiao-esp32c6
pio run -e nanoesp32c6-n16-aws
pio run -e xiao-esp32c6-aws
```

GPS reception is physically validated separately on both boards. The N16 also
passed simultaneous dual-CAN reception and a normal-road WiFi/AWS IoT run with a
unique certificate and live portal ingestion. The current firmware does not yet
claim WebUI or OTA runtime support. The partition tables reserve dual OTA slots
and LittleFS; this layout evidence is not an OTA rollback test.

Default decoder assignment is Standard-CAN V1 - Pioneer on CAN1 and Display-CAN
on CAN2. Both channels accept any registered decoder profile at runtime; these are
safe defaults for the intended dual-CAN adapter, not decoder-engine restrictions.

## Mobile test capture

The capture starts automatically from an empty state at every boot and records a
bounded summary in RAM without transmitting on either CAN bus. It tracks the
known Standard-CAN charge/current/voltage candidates and Display-CAN SOC, speed,
power-display and plug candidates. Use the serial console at 115200 baud:

```text
drive reset
drive dump
drive trace
```

`drive reset` starts a new observation window. `drive dump` prints the accumulated
minima, maxima, status-bit changes and representative raw frames. The result is
lost if the C6 is reset or loses USB power, so keep it powered until the dump has
been collected.

`drive trace` prints up to 600 synchronized samples at 200 ms intervals (the most
recent 120 seconds): speed, Standard-CAN current/voltage/status, derived power,
Display-CAN power byte and plausibility result. This is intended for the bounded
flat-road traction/regeneration test. The completed 2026-08-06 test confirmed
negative pack current during traction, positive pack current during regeneration
and increased electrical regeneration under light braking.

## WiFi and AWS qualification

Configure WiFi over the serial console without echoing the password:

```text
wifi set MySSID|MyPassword
wifi status
aws status
```

Restart after changing WiFi. AWS builds load unique X.509 credential files from
LittleFS `/aws`; credentials are not linked into the application image. The
shared upload helper accepts `esp32-c6` and defaults to
`nanoesp32c6-n16-aws`, with `xiao-esp32c6-aws` selectable explicitly.

The C6 publisher sends Display-CAN, confirmed Pioneer pack voltage/current/power,
plug/charge state, provisional cell extrema and valid GPS coordinates using the
same topic contract as the established devices.
