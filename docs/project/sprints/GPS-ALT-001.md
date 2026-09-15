# GPS-ALT-001 — GNSS altitude telemetry

> **Status:** Complete — physical C6 publication through AWS and local MQTT accepted
>
> **Started:** 2026-09-15

## Objective

Publish the receiver altitude as `location/altitude_m` for Grafana, local MQTT
and MOT Cloud without creating a second GPS implementation.

## Scope and result

- C6 N16 and XIAO use the shared `MotGps::altitudeMeters()` value for AWS and
  secure local MQTT.
- LilyGO T-A7670X and T-SIM7670G-S3 publish the same suffix on their maintained
  AWS path; the existing T-A7670X legacy path is covered as well.
- The value is published only with a valid GPS fix and uses metres with one
  decimal transport precision.
- WROOM is intentionally not included in the release gate. Its current 4 MB AWS
  image is already 4,516 bytes over the existing OTA slot; changing partitions
  or sacrificing recovery margin is outside this sprint.

## Validation

- `nanoesp32c6-n16`: build passed, 26.8% of the 5 MiB application slot.
- `xiao-esp32c6`: build passed, 81.6% of the 1,703,936-byte application slot.
- `T-A7670X-AWS`: build passed, 78.5% of its application slot.
- `T-SIM7670G-S3-Standard-AWS`: build passed, 25.2% of its application slot.
- Static publisher contract test passes.

Physical C6 publication was accepted on 2026-09-15 with plausible outdoor
altitude values reaching both MOT Cloud/AWS and the secure local MQTT path. This
closes GPS-ALT-001 at the firmware telemetry boundary. Filtered ascent/descent,
portal presentation and journey summaries remain owned by JNY-ALT-001.
