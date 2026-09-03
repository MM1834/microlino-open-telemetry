# FW-CHECK-001 — Portal Firmware Compatibility Check

> **Status:** Superseded by WEBFLASH-001 on 2026-09-01
>
> **Priority:** Retained as design history
>
> **Scope:** Informational compatibility check only

## Objective

Let the authenticated portal compare the firmware reported by an adapter with the
latest approved firmware for that exact compatible build target. The check must
not infer compatibility from a generic chip family or offer remote installation.

## Proposed contract

In addition to the existing `system/firmware_version`, maintained firmware should
publish stable machine-readable identity fields such as:

- `system/platform`, for example `esp32-c6`;
- `system/board`, for example `nanoESP32-C6-N16` or `Seeed-XIAO-ESP32-C6`;
- `system/firmware_target`, for example `c6-n16-aws`.

An independently versioned release manifest should map each firmware target to
its latest approved version and optional minimum supported version. Initially the
manifest may be a static portal asset generated or validated from the firmware
release process; it must not become an unrelated manually maintained second
source of truth.

## Portal states

The Status section should distinguish at least:

- current;
- compatible update available;
- older than the supported minimum;
- check unavailable because an older adapter does not report a target;
- manifest unavailable, without hiding the installed version.

Retained firmware identity must follow the existing device-freshness semantics so
an offline adapter is not presented as having completed a current live check.

## Boundaries

- No portal-triggered update, remote OTA or AWS IoT Jobs in this sprint.
- No public firmware download is implied.
- No comparison between different board, flash-layout or transport targets.
- Existing local OTA chip/flash guards remain authoritative for installation.
- Older firmware remains visible and usable when the new identity fields are
  absent.

## Acceptance outline

- N16 and XIAO resolve to different compatible targets even when they share a
  human-readable firmware release.
- Current, update-available, unsupported and unavailable states have deterministic
  tests.
- Missing/stale manifest or adapter metadata fails informationally, not as a
  dashboard or telemetry error.
- Desktop and smartphone presentation is accepted without increasing the primary
  overview footprint.

## Start gate

The maintainer explicitly started the broader controlled portal Web Flasher on
2026-09-01. [WEBFLASH-001](WEBFLASH-001.md) now owns the canonical release
manifest, hardware-aware pre-write gate, protected artifact delivery and local
USB installation. Do not implement this older informational proposal separately.
