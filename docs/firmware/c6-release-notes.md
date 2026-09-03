# ESP32-C6 Firmware Release Notes

> **Current release:** `C6-001-REV15-AWS`
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
- **N16 firmware images:** REV15 deliberately rejects a 16 MB N16 OTA image on a
  4 MB XIAO. XIAO requires the `xiao-esp32c6` image.

### Supported with a reduced limit

- **Offline History cache:** supported, but capped at 128 KiB instead of the
  N16's 256 KiB because XIAO has only 4 MB flash.
- **Firmware growth:** the XIAO image must remain below 85% of one OTA application
  slot. REV15 remains subject to the 85% gate, so additional N16 features are not automatically
  enabled on XIAO.

### Still supported in the current XIAO image

- passive Dual-CAN and Display/V1/V2 decoder selection;
- GPS, Home/Mobile WiFi fallback and protected local AP;
- AWS IoT telemetry, ABRP and optional offline History backfill;
- authenticated WebUI, guided onboarding, backup/restore and factory reset;
- local OTA with the REV13+ chip/flash compatibility validation.

XIAO remains a supported compatibility target, but it does not have the same
extended vehicle Dual-CAN/AWS field qualification as the N16 production pilot.
