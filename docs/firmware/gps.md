# GPS

The LilyGO firmware uses an external L76K GPS receiver and TinyGPSPlus-style parsing.

## Pins

| Function | GPIO |
|---|---:|
| GPS RX | 22 |
| GPS TX | 21 |
| PPS | 23 |
| WAKEUP | 19 |

## Telemetry behavior

GPS is independent from vehicle CAN. Location and system topics can therefore be tested without connecting the device to OBD/CAN.

The firmware should only publish latitude/longitude when the GPS fix is valid.

## Time

ABRP requires a valid Unix timestamp. Earlier ABRP work used NTP after WiFi connection and withheld ABRP sends until time passed a validity threshold.

The current LilyGO GPS status also exposes UTC derived from GPS when available. The final time-source policy should be documented after the ABRP/LTE work is completed.

## Status fields

Typical fields include:

- receiver seen,
- fix valid,
- location age,
- satellite count,
- HDOP,
- latitude/longitude,
- speed,
- UTC.
