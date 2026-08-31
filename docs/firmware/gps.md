# GPS

> **Status:** Source-confirmed optional capability; C6-N16 enable/disable physically validated
>
> **Audience:** Firmware developer, hardware reviewer and beta-support author

Both targets use the shared `MotGps` NMEA parser. Detection and valid fix are
different states: a receiver is detected after valid NMEA input, while coordinates
are published only with a valid location fix.

| Target | RX | TX | Baud | Hardware intent |
|---|---:|---:|---:|---|
| ESP32-WROOM | 16 | 17 | 9600 | Optional external GPS |
| LilyGO | 22 | 21 | 9600 | External L76K; PPS 23, wakeup 19 |
| nanoESP32-C6-N16 | 20 | 21 | 9600 | Same optional receiver and detection states as WROOM |
| Seeed XIAO ESP32-C6 | 17 | 16 | 9600 | Candidate compact-board wiring |

DA37+DA10 is the preferred optional receiver for new C6 pilot assemblies. It uses
the existing 3.3 V, 9600-baud NMEA UART contract and requires no firmware variant.
Its separate antenna improves enclosure placement; PPS remains unconnected.

The nanoESP32-C6-N16 UART wiring was physically checked on 2026-08-06: receiver
TX to GPIO20 and receiver RX to GPIO21 produced continuously increasing UART-byte
counts and checksum-valid NMEA sentences. The indoor test confirmed module
detection but not a satellite fix.

The same assembly subsequently reached and retained `GPS_FIX` under open sky,
confirming the complete receiver-to-UART-to-parser location-fix path. Combined
GPS, dual-CAN and WiFi operation remains a C6 qualification gate.

The XIAO wiring was physically checked outdoors on 2026-08-06. Receiver TX on
D7/GPIO17 and receiver RX on D6/GPIO16 produced checksum-valid NMEA and a valid
fix after a cold-start interval, with the character count exceeding 59,000. CAN
was not connected and WiFi was not configured during this test.

GPS is optional in the shared readiness model and does not block basic readiness.
ESP32-WROOM beta devices may therefore be delivered with or without GPS using the
same intended firmware line.

All maintained firmware targets persist a `gpsEnabled` setting. It defaults to
`true`, preserving existing installations. After checksum-valid NMEA has identified
a receiver, Configuration and the local onboarding wizard expose an authenticated
GPS switch. The control also remains visible while disabled so the receiver can be
re-enabled. Disabling takes effect after reboot and stops UART initialization,
parsing and GPS telemetry. It only removes module power on hardware with a supported
power/wakeup control line; an externally powered C6 or WROOM receiver remains
electrically powered.

The complete control path was physically accepted on a nanoESP32-C6-N16 with
`C6-001-REV6-AWS` on 2026-08-20. Both Configuration and the onboarding wizard
successfully disabled and re-enabled GPS. While disabled, Runtime Diagnostics
reported `GPS_DISABLED`, zero received characters and no fix. The exported
configuration included `gpsEnabled`; the common import path restores the same
persisted setting.

Demo adapter B025 physically validated DA37+DA10 on 2026-08-25 without additional
firmware changes or added support capacitors. UART input produced continuous valid
NMEA, GNSS time set the system clock and the receiver progressed from
`GPS_DETECTED` to a stable `GPS_FIX` outdoors after an approximately two-minute
cold start. Support capacitors remain part of the recommended final assembly.

Location MQTT topics are emitted only while the current fix is valid. Retained AWS
coordinates may remain as last-known values when a fix disappears; consumers use
receive metadata/freshness rather than presence alone.

## Validation needs

- no-receiver detection timeout/state;
- receiver present without fix;
- first fix and loss/recovery;
- coordinate, speed, satellite, HDOP and age accuracy;
- WROOM, XIAO and LilyGO physical enable/disable UI regression;
- LilyGO GPS interaction with modem and power conditions.

## Related documents

- [MQTT topics](../api/mqtt-topics.md)
- [ESP32-WROOM](../hardware/esp32-wroom.md)
- [LilyGO](../hardware/lilygo-t-a7670.md)
