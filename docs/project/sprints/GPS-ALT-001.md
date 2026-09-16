# GPS-ALT-001 — GNSS altitude telemetry

> **Status:** Complete — physical C6 publication through AWS and local MQTT accepted
>
> **Started:** 2026-09-15
>
> **Field acceptance:** 2026-09-16 on N16 with REV26

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
- A field observation after acceptance showed that a degraded indoor fix could
  still carry a formally valid but continuously drifting altitude. The shared
  parser now requires a fresh position and altitude (at most five seconds old),
  at least four satellites and HDOP no greater than 5.0 before exposing height.
  Diagnostics include `altitudeValid` and `altitudeAgeMs`; C6 and LilyGO omit
  `location/altitude_m` whenever this separate quality gate fails.
- A second field observation on REV21 proved that five-satellite indoor multipath
  could still pass that first gate. REV22 added fresh Display-CAN motion plus
  tighter GNSS thresholds, but a road test received no height despite confirmed
  CAN speeds up to 38 km/h and a later 12-satellite fix. REV23 therefore retains
  the decisive fresh Display-CAN motion gate while restoring the field-proven
  REV21 GNSS thresholds. Invalid or stationary height is omitted; no validity
  topic is added.
- REV24 restored moving altitude but still accepted a severe drop while the car
  was rolling into the garage: AWS stored 107.1 m at 09:08:20 and the first
  recorded zero speed followed at 09:08:45. REV25 therefore requires six
  satellites and adds a stateful horizontal-distance/grade filter: at least 10 m
  displacement and no more than 10 m plus 50 percent of that distance as height
  change. Rejected samples do not move the accepted anchor.
- The first REV25 garage-exit test exposed a cold-start exception: the first
  moving value was accepted unconditionally and could poison the anchor. REV26
  keeps that first point provisional and publishes nothing until a second point
  at least 10 m away passes the same plausibility check. Failed qualification
  restarts from the newer provisional point and cannot publish it.
- WROOM is intentionally not included in the release gate. Its current 4 MB AWS
  image is already 4,516 bytes over the existing OTA slot; changing partitions
  or sacrificing recovery margin is outside this sprint.

## Validation

- `nanoesp32c6-n16`: REV26 build passed, 27.0% of the 5 MiB application slot.
- `xiao-esp32c6`: REV26 build passed, 82.3% of the 1,703,936-byte application slot.
- `T-A7670X-AWS`: REV26 build passed, 79.1% of its application slot.
- `T-SIM7670G-S3-Standard-AWS`: REV26 build passed, 25.4% of its application slot.
- Static publisher contract test passes.

Physical C6 publication was accepted on 2026-09-15 with plausible outdoor
altitude values reaching both MOT Cloud/AWS and the secure local MQTT path. This
was followed by the complete REV26 road-to-garage acceptance on 2026-09-16: a
short drive produced valid height and waiting inside the garage did not replace
it with a drifting indoor value. This closes GPS-ALT-001 at the firmware telemetry boundary. Filtered ascent/descent,
portal presentation and journey summaries remain owned by JNY-ALT-001.
