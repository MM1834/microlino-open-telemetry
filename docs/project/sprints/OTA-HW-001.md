# OTA-HW-001 — Hardware-aware local OTA guard

> **Status:** Complete — REV13 positive and negative OTA acceptance physically passed
>
> **Date:** 2026-08-31

## Objective

Reject a local OTA image before the first flash write when its ESP chip family or
declared flash size does not match the running adapter. The priority case is the
16 MB nanoESP32-C6-N16 versus the 4 MB XIAO ESP32-C6.

## Scope

- inspect the standard Espressif image header already present at the beginning of
  every application binary;
- compare image magic, chip ID and declared flash size with the running device;
- call `Update.begin()` only after that comparison passes;
- show a specific, user-readable rejection reason without rebooting;
- preserve authentication, same-origin protection, OTA enablement and existing
  successful-update behaviour;
- apply the shared guard to C6, WROOM and LilyGO without a new firmware package
  format or external manifest.

The guard detects the supported board distinctions that differ by chip family or
flash size. It does not claim to distinguish hypothetical boards with the same
ESP chip and identical flash size but different pin wiring; that would require a
separately signed or embedded MOT board manifest.

## Delivery slices

| Slice | Outcome | Status |
|---|---|---|
| OTA-HW-001.A | Standard ESP image-header parser and mismatch rejection before flash writes | Complete |
| OTA-HW-001.B | Focused valid/malformed/chip/flash mismatch contracts | Complete |
| OTA-HW-001.C | N16, XIAO, WROOM and LilyGO build/resource validation | N16, XIAO, LilyGO and WROOM base pass; WROOM AWS exceeds its pre-existing slot boundary |
| OTA-HW-001.D | Physical valid-image and wrong-image rejection on an N16 | Complete |

## Acceptance

- an N16 rejects the XIAO image before `Update.begin()`;
- a XIAO rejects the N16 image before `Update.begin()`;
- an ESP32 target rejects an ESP32-C6 image and vice versa;
- a matching application image still installs normally;
- rejection leaves the running firmware and configuration unchanged;
- added static RAM and application flash remain negligible and all maintained
  firmware build gates pass.

## Risk boundary

The ESP header remains protected by the normal Espressif image checksum/hash
validation after writing. This sprint adds target compatibility, not image
signing, anti-rollback or cryptographic release authorization.

## Repository evidence

REV13 validates the first 24 bytes of the uploaded standard Espressif application
image before calling `Update.begin()`. Invalid magic, a different chip ID or a
different declared physical flash size returns HTTP 400 and does not open or
write the OTA partition. The shared path covers C6 and WROOM; LilyGO applies the
same guard to its existing local route.

The final REV13 N16 build uses 58,616 bytes RAM and 1,379,288 bytes application
flash, an increment of 16 bytes RAM and 1,556 bytes flash over REV11. XIAO uses
57,876 bytes RAM and 1,365,246 bytes application flash; its OTA binary occupies 83.50%
of the slot and passes the 85% gate. LilyGO AWS and the WROOM base environment
build successfully. The current WROOM AWS source set exceeds its 1,310,720-byte
application slot by 3,344 bytes and therefore remains blocked independently of
the C6 rollout; no oversized WROOM image is accepted for release.

The first physical matching-image attempt on REV12 failed safely before a flash
write but exposed an incorrect ESP32-C6 compile-target macro: the image reported
chip ID 13 while the running target resolved to the unknown sentinel 65535. REV13
uses the Arduino/IDF macro `CONFIG_IDF_TARGET_ESP32C6`; the focused contract now
guards against reintroducing the incorrect underscore form. Physical matching-
and wrong-image retests remain open.

Physical REV13 acceptance then passed on the N16 adapter. Uploading the matching
REV13 N16 application image completed normally. Uploading the XIAO C6 image was
rejected before writing with `Firmware flash-size mismatch (image 4 MB, adapter
16 MB)`. Uploading the LilyGO ESP32 image was rejected before writing with
`Firmware chip mismatch (image 0, adapter 13)`. Both rejection pages confirmed
that the running firmware was unchanged. This supplies the required positive
control as well as independent flash-geometry and chip-family negative controls;
OTA-HW-001 is complete.
