# ESP32-C6 Firmware Release Notes

> **Current repository release:** `C6-001-REV26-AWS`
>
> **Current field-accepted release:** `C6-001-REV26-AWS`

### `C6-001-REV26-AWS`

REV26 closes the cold-start exception found when leaving the garage on REV25.
The first moving altitude after boot is now provisional and never published. A
second point at least 10 m away must pass the same distance-relative height check
before becoming the accepted anchor. Failed qualification restarts from the
newer provisional point without sending it, so a degraded initial fix cannot
poison the filter. The parallel C6 status-LED work remains intact.
Physical N16 acceptance passed on 2026-09-16: a short drive established valid
height, and subsequent waiting inside the garage produced no drifting replacement
value. The onboard GPIO8 RGB LED was also accepted on the physical N16 for boot,
WiFi, AWS IoT and local MQTT states. Its red Dual-CAN startup-error indication is
covered by source/build contracts but was not destructively provoked during the
field test.

The exact N16 application from commit `b427650` was activated in the controlled
WebFlash catalog through replacement-free Change Set
`webflash-rev26-20260916`. The private AES256-encrypted, versioned object is
1,476,224 bytes with SHA-256
`597c00565204d0531ec8b46d676071f3dc4dab1b4949a6ebe98fdcfaed0882d0`.
All three previously active N16 pilot grants were explicitly and atomically
renewed for the REV26 identity while retaining their existing expirations; each
renewal has a successful audit record. The XIAO catalog entry remains on REV21.

### `C6-001-REV25-AWS`

REV25 addresses the remaining height drop while rolling into the garage. It
requires at least six satellites and retains REV24's CAN-motion gate. After the
first moving anchor, height is published only after at least 10 m horizontal
movement and when the height change remains within 10 m plus 50 percent of that
distance. Rejected measurements do not advance the anchor, preventing degraded
indoor fixes from ratcheting the accepted height downward.

### `C6-001-REV24-AWS`

REV24 increases the socket inactivity timeout only for an authenticated,
same-origin local OTA upload from the ESP32 framework default of 5 seconds to
30 seconds. This tolerates transient packet loss and retransmission on weak WiFi
without changing Home/Mobile/AP selection, ordinary WebUI timeouts or image
validation. An actual connection loss still requires restarting the upload.

### `C6-001-REV23-AWS`

REV23 corrects an over-restrictive REV22 altitude gate. The first REV22 road test
recorded Display-CAN speeds up to 38 km/h and later 12 satellites but published
no height. REV23 keeps the decisive fresh CAN-motion requirement that suppresses
stationary garage drift, while restoring REV21's proven four-satellite and HDOP
5.0 GNSS thresholds. Invalid or stationary height remains omitted without a
separate validity topic.

### `C6-001-REV22-AWS`

REV22 closes the remaining indoor-altitude path observed with REV21. Satellite
count and HDOP must now be fresh as well as position and altitude, the height
gate requires at least six satellites and HDOP no greater than 2.5, and C6 plus
LilyGO publish height only with fresh Display-CAN motion above 1 km/h. Invalid or
stationary height is omitted; no separate validity topic is introduced.
The focused altitude contract and the N16, XIAO, T-A7670X and
T-SIM7670G-S3 AWS build gates pass. Physical road-to-garage acceptance remains
open.

### `C6-001-REV21-AWS`

REV21 hardens the REV20 altitude publisher after an indoor field observation.
A general fresh location fix is no longer sufficient for height: location and
altitude must both be at most five seconds old, at least four satellites must be
reported and HDOP must be no greater than 5.0. When that separate quality gate
fails, C6 and LilyGO omit `location/altitude_m` instead of publishing a drifting
indoor value. GPS diagnostics expose `altitudeValid` and `altitudeAgeMs`.

The immutable N16 and XIAO application images are active in the controlled
WebFlash catalog. Change Set `webflash-rev21-20260915` updated only the existing
Lambda environment, its exact S3 object permissions and the dependent API
integration, all without replacement. The stack returned to `UPDATE_COMPLETE`;
the Lambda is `Active`/`Successful` and both private artifacts are AES256
encrypted and versioned. Existing user grants remain release-bound and therefore
must be explicitly issued or renewed for REV21.

### `C6-001-REV20-AWS`

REV20 publishes raw valid-fix GPS altitude as `location/altitude_m` through MOT
Cloud/AWS and the optional secure local MQTT service. Physical C6 testing
confirmed plausible outdoor altitude on both transports. The N16 and XIAO build
gates remain part of the release validation; WROOM remains outside this release
boundary because its existing image exceeds its OTA slot.

REV20 includes the now accepted REV19 Standard-CAN V2 plug and charging-state
decoder described below.

### `C6-001-REV19-AWS`

REV19 separates the newly verified Standard-CAN V2 charging-state decoder from
the previously accepted REV18 release identity. On the tested newer-generation
large-battery vehicle, Standard-CAN `0x60D` byte 1 bit 3 identifies a connected
charge cable, while `0x610` byte 0 distinguishes unplugged (`0x01`), plugged and
idle (`0x05`) and actively charging (`0x02`). Pack voltage, current and derived
power remained plausible during controlled 6 A and 8 A charging tests, and both
CAN channels remained error-free.

The plug and active-charging portion is field-accepted after the original
large-battery test and a subsequent V2 confirmation. This does not claim that
traction and regeneration have been validated on every newer-generation
battery/BMS variant.
>
> **Targets:** nanoESP32-C6-N16 and Seeed XIAO ESP32-C6

This page summarizes the operational C6 revision sequence beginning with the
first XIAO-capable `C6-001-AWS` pilot. Some intermediate field revisions were
built and validated between consolidated repository commits; the entries below
therefore describe their functional boundary rather than claiming one Git commit
per revision.

## Revision history

### `C6-001-AWS` / `C6-001-REV1-AWS`

- first shared N16/XIAO ESP32-C6 firmware line;
- two passive 500 kbit/s TWAI/CAN inputs with independent decoder profiles;
- Display-CAN, Pioneer Standard-CAN V1 and provisional V2 profiles;
- WiFi/AWS telemetry, GPS, CAN scan and bounded drive capture;
- separate 16 MB N16 and 4 MB XIAO partition/pin profiles.

### `C6-001-REV2-AWS`

- protected setup/fallback access point and authenticated local WebUI;
- configuration backup/restore and factory reset;
- local OTA, failed-image recovery and physical USB administrator recovery;
- cooperative WiFi/CAN/GPS/AWS runtime hardening.

### `C6-001-REV3-AWS`

- expanded WiFi/AWS/TLS, reset, heap and disconnect diagnostics;
- weak-link and mesh/BSSID observation support;
- diagnostic field revision; no intended telemetry-contract break.

### `C6-001-REV4-AWS`

- ABRP TLS object lifetime and heap-leak correction;
- ABRP memory gates and asynchronous/manual status controls;
- associated Home WiFi is no longer abandoned only because of weak RSSI;
- bounded Home/Mobile/AP recovery retained.

### `C6-001-REV5-AWS`

- restored unconditional Home-WiFi priority when the configured Home SSID is
  visible;
- automatic Mobile-to-Home return without depending on an RSSI threshold;
- AWS, ABRP, Dual-CAN and GPS coexistence field-validated on N16.

### `C6-001-REV6-AWS`

- persistent GPS enable/disable control in Configuration and onboarding;
- explicit `GPS_DISABLED` diagnostics and backup/restore support;
- service and Standard-CAN plausibility hardening.

### `C6-001-REV7-AWS`

- receive-only/listen-only CAN contract aligned across maintained adapters;
- optional offline SOC/speed History cache and acknowledged AWS backfill;
- N16 cache limit 256 KiB, XIAO cache limit 128 KiB;
- no GPS/location storage in the offline cache.

### `C6-001-REV8-AWS`

- Standard-CAN V1/V2 becomes authoritative for plug and charging state;
- Display-CAN charging frames `0x603` and `0x604` are ignored whenever a
  Standard-CAN profile is configured;
- prevents false journey completion from conflicting Display-CAN charge data.

### `C6-001-REV9-AWS`

- guided local onboarding refined into a persistent seven-step flow;
- first setup separated from WiFi, CAN and optional-service configuration;
- protected device AP retained until onboarding completes;
- configuration-preserving migration for already deployed adapters.

### `C6-001-REV10-AWS`

- Standard-CAN V2 decoding expanded for pack voltage, cell extrema, SOC, SOH,
  current and vehicle power;
- MOT Cloud and ABRP can be controlled independently without deleting AWS
  credentials;
- ABRP credential deletion and additional AWS/TLS diagnostics;
- charging/status freshness and floating-point SOC handling corrected.

### `C6-001-REV11-AWS`

- N16 RAM-only journey-energy accumulator;
- separate drawn and regenerated Wh counters with stable journey identity;
- checkpoints at journey start, every 60 seconds and at stop/charge boundaries;
- backend can label journey email as `Firmware-Zähler`, with telemetry fallback;
- intentionally disabled on XIAO.

### `C6-001-REV12-AWS` — withdrawn

- first pre-write OTA chip/flash compatibility guard;
- rejected images safely, but used the wrong ESP32-C6 compile-target macro and
  therefore also rejected a matching N16 image;
- requires one-time USB update to REV13 when REV12 is already installed.

### `C6-001-REV13-AWS`

- corrected ESP32-C6 target detection;
- validates Espressif image magic, chip family and declared physical flash size
  before opening the OTA partition;
- physical positive test: matching N16 image accepted;
- physical negative tests: XIAO image rejected for 4/16 MB mismatch and LilyGO
  image rejected for ESP32/ESP32-C6 mismatch;
- failed validation leaves the running firmware unchanged.

### `C6-001-REV15-AWS`

- adds Pioneer-only `0x48D` decoding as the optional
  `bms/soc_internal` and `bms/soc_display` topics;
- keeps `display/soc` as the canonical product SOC and never substitutes either
  Pioneer diagnostic value;
- publishes the independent V2 `0x1B1` values as `bms/standard_soc` and
  `bms/soh_percent` when that profile receives them;
- leaves all optional SOC/SOH values absent when their profile-specific frame is
  unavailable.

### `C6-001-REV17-AWS`

- publishes the configured CAN1 and CAN2 decoder keys as retained AWS birth
  state alongside the firmware version;
- seals and prioritizes the final N16 journey-energy checkpoint immediately when
  fresh Standard-CAN plug or charging state becomes active;
- treats `Speed = 0` only as a stop candidate and seals the counter after ten
  continuous stopped minutes, not on the first zero sample;
- preserves a sealed counter until its final MQTT triplet is accepted before a
  later movement starts a new counter identity;
- leaves the XIAO journey-counter boundary unchanged and disabled.

REV16 was an internal, unpublished journey-fix worktree state. Its revision
counter had been raised while the embedded runtime release string still reported
REV15, so it was deliberately never packaged or activated for WebFlash. REV17 is
the first immutable release containing both that journey fix and the CAN-profile
telemetry.

### `C6-001-REV18-AWS`

- publishes the compile-time hardware identifier as retained AWS birth state at
  `system/board`;
- renames the V1 stable decoder key to
  `standard-can-v1-pioneer-gen1-midrange` and its local UI label to
  `Standard-CAN V1 - Pioneer / Gen1 Mid-Range`;
- preserves numeric decoder profile value `2` and accepts the former
  `standard-can-v1-pioneer` command alias, so stored configuration and service
  workflows remain compatible.

### `C6-001-REV14-AWS`

- publishes `journey/energy_counter_id` as a valid JSON string;
- enables the journey backend to combine the ID with drawn/regenerated Wh values
  instead of falling back to the telemetry estimate;
- keeps the N16 RAM-only journey counter and REV13 OTA compatibility guard
  otherwise unchanged.

## XIAO feature boundary

### Not supported on XIAO

- **Firmware journey-energy counter (REV11+):** the RAM Wh accumulator and its
  `journey/energy_*` checkpoints are N16-only. XIAO journey emails continue to use
  the compatible backend `Telemetrie-Schätzung` path.
- **N16 firmware images:** REV18 deliberately rejects a 16 MB N16 OTA image on a
  4 MB XIAO. XIAO requires the `xiao-esp32c6` image.

### Supported with a reduced limit

- **Offline History cache:** supported, but capped at 128 KiB instead of the
  N16's 256 KiB because XIAO has only 4 MB flash.
- **Firmware growth:** the XIAO image must remain below 85% of one OTA application
slot. REV18 remains subject to the 85% gate, so additional N16 features are not automatically
  enabled on XIAO.

### Still supported in the current XIAO image

- passive Dual-CAN and Display/V1/V2 decoder selection;
- GPS, Home/Mobile WiFi fallback and protected local AP;
- AWS IoT telemetry, ABRP and optional offline History backfill;
- authenticated WebUI, guided onboarding, backup/restore and factory reset;
- local OTA with the REV13+ chip/flash compatibility validation.

XIAO remains a supported compatibility target, but it does not have the same
extended vehicle Dual-CAN/AWS field qualification as the N16 production pilot.
