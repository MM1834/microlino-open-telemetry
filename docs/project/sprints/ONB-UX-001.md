# ONB-UX-001 — Guided C6 Local Onboarding

**Status:** Completed
**Accepted:** 2026-08-25

## Objective

Make first-time C6 setup understandable without administrator ping-pong while
preserving the protected local access point, credentials, AWS identity and later
configuration access.

## Delivered

- reduced the one-time `setup` page to the administrator/hotspot credential and
  required the new password twice;
- unified first-run WiFi, CAN, telemetry-service and validation steps in the
  authenticated wizard, with progress persisted across required reboots;
- kept the protected `MOT-xxxx` access point available until explicit onboarding
  completion and retained the normal fallback behavior afterwards;
- added 2.4 GHz and iPhone compatibility guidance, active WiFi profile, SSID and
  local-IP handoff information without exposing stored secrets;
- avoided a reboot when the CAN profile is unchanged;
- separated History cache from optional ABRP configuration and clarified device
  and telemetry validation;
- handled a completely erased LittleFS partition safely while leaving corrupted,
  non-empty storage fail-closed;
- updated the operator onboarding guide and reproducible one-page pilot handout.

## Acceptance evidence

- the rebuilt flow was exercised repeatedly on factory-erased demo unit B025;
- active network and IP display, password transition, wizard text and reboot
  continuation were accepted during the physical walkthrough;
- B021, B023 and B024 received the same application firmware while retaining
  their prepared identities and configuration;
- repository contract tests cover authentication, wizard progression, dual-WiFi
  behavior, CAN reboot behavior, final handoff text and safe LittleFS recovery;
- both supported ESP32-C6 PlatformIO environments build successfully.

## Follow-up boundary

History, email and SMS activation remain administrator-controlled optional
services. Removing that remaining coordination belongs to the future onboarding
admin tool and does not reopen this firmware UX sprint.
