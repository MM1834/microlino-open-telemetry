# Microlino Display CAN

Microlino Display CAN is used as the baseline CAN source for MOT.

It appears to be available on both Pioneer and newer Microlino models, making it suitable as the common base decoder.

## Confirmed Values

### CAN ID 0x602

Confirmed / currently used:

| Field | Decode | Status |
|---|---|---|
| SOC | `B0 / 2.0` | confirmed |
| Speed | `B1 / 2.0` | confirmed from previous testing |
| Odometer | `((B7 & 0x0F) << 24) | (B6 << 16) | (B5 << 8) | B4`, divided by `1024.0` | confirmed |
| Range | calculated from SOC | derived |

### CAN ID 0x603

Current candidate values:

| Field | Decode | Status |
|---|---|---|
| Power display | `B4` | candidate / useful for charging detection |
| Charging | threshold on power display | needs calibration |

Initial observations:

- Power value around 9 was observed while not plugged in / not charging.
- Power value around 64 was observed while charging.
- The old threshold `> 4` is likely too low.

### CAN ID 0x604

Current candidate values:

| Field | Decode | Status |
|---|---|---|
| Plugged | `B4 & 0x10` | unconfirmed |

## Notes

Trip decoding is not yet included in stable telemetry.

It may represent the digits displayed on the vehicle display and requires additional validation before publishing as a stable value.
