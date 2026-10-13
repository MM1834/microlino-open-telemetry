# DISPLAY-SAFETY-001 — Local emergency-instrument warnings

> **Status:** Complete; paired REV41/Display 1.1.0 release installed and WebFlash active
>
> **Created:** 2026-10-13

## Objective

Extend the 1.28-inch MOT Display fallback instrument with parking-brake and
warning state already present on Display-CAN frame `0x603`, without exporting
those values to AWS or local MQTT.

## Contract

The paired adapter/display contract intentionally changes together because only
two controlled pairs exist. `DrivingStatusV1` grows from 12 to 14 bytes and now
contains a 16-bit flag field plus the raw warning byte. Mixed Display 1.0/1.1
and REV40/REV41 pairs do not accept this record and are not a supported rollout.

| DBC signal | Decode | Display |
|---|---|---|
| `0x603_Feststellbremse` | Intel bit 20: `data[2] & 0x10` | red `P` symbol |
| `0x603_Byte3_u8` | byte 3; active when non-zero | `! WARNUNG 90` |
| `0x603_Bit43` | Intel bit 43: `data[5] & 0x08` | `! HBL / AH` |
| `0x603_Warnung_Antrieb_gelb` | Intel bit 45: `data[5] & 0x20` | `! ANTRIEB GELB` |

Active warning text temporarily replaces range and flow-state text on the main
screen. Speed, power, SOC, ring and direction indicators remain visible. The
decoder retains the raw byte for diagnosis and asserts the display warning for
any non-zero value. All values expire with the existing bounded
Display-CAN freshness window.

## Validation

- 79 focused display, service-parity and WebFlash tests pass.
- Canonical N16 REV41 build passes.
- MOT Display 1.1.0 build passes.
- The USB-connected adapter/display pair was installed and loader-verified.
- The display layout marker was restored after USB upload and read back with
  SHA-256 `007908ae4fbba301897798e8aa8a377a6907d429ab133ba144e7c404d8c81623`.
- Display factory and application-update WebFlash catalogs are active on 1.1.0;
  the normal N16 WebFlash catalog remains REV38.
- The controlled pair was recovered after a temporary USB-port assignment mix-up:
  the original Waveshare image proved the display hardware, Display 1.1.0 was
  reinstalled, and the N16 adapter was fully restored to REV41 with its existing
  `mot-ml-9c27db` / `ml-pilot-039` ACTIVE credential set.
- Adapter boot diagnostics confirm both CAN controllers, GPS and Display BLE;
  the maintainer will perform the final vehicle warning-signal spot check before
  shipment as an operational acceptance step.
