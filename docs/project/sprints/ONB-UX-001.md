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
  authenticated wizard, with progress persisted throughout the flow;
- kept the protected `MOT-xxxx` access point available until explicit onboarding
  completion and retained the normal fallback behavior afterwards;
- added 2.4 GHz and iPhone compatibility guidance, active WiFi profile, SSID and
  local-IP handoff information without exposing stored secrets;
- saves GPS, WiFi, CAN and service pages without intermediate restarts, reviews
  their non-secret values, and applies them with one consolidated restart before
  real-IP and runtime validation;
- separated History cache from optional ABRP configuration and clarified device
  and telemetry validation;
- handled a completely erased LittleFS partition safely while leaving corrupted,
  non-empty storage fail-closed;
- updated the operator onboarding guide and reproducible one-page pilot handout.

## Pilot feedback refinement

Post-acceptance pilot feedback made displayed local HTTP addresses clickable,
added immediate visual progress feedback to wizard actions, reduced and indented
the optional GPS control, and presents CAN2 as the fixed Microlino Display-CAN
wiring in the wizard. The decoder-profile model remains extensible for a later
explicit display variant such as a verified miles decoder.

On 2026-09-01 the N16 flow was further simplified to let users complete every
configuration page before restarting. Future SSIDs are shown on the review page;
the router-assigned IP is deliberately shown only after the real connection has
been attempted during the single apply restart. The verified N16 image was then
installed over USB on demo adapter B025 (`MOT-4085D9`) without erasing its NVS,
LittleFS or credential partitions; the post-flash console confirmed the expected
board identity, fallback AP and running CAN/GPS/network services.
Follow-up acceptance feedback moved the hardware-step action below the GPS toggle
and made both the review and restart pages explicitly instruct the user to reconnect
to `MOT-xxxx`, reopen `http://192.168.4.1/` and sign in as `admin` after the single
restart. The text also states unambiguously that the protected AP remains active
until onboarding is completed.

## Acceptance evidence

- the rebuilt flow was exercised repeatedly on factory-erased demo unit B025;
- active network and IP display, password transition and the earlier reboot
  continuation were accepted during the physical walkthrough; physical acceptance
  of the consolidated one-restart refinement remains pending;
- B021, B023 and B024 received the same application firmware while retaining
  their prepared identities and configuration;
- repository contract tests cover authentication, wizard progression, dual-WiFi
  behavior, the consolidated restart contract, final handoff text and safe LittleFS
  recovery;
- both supported ESP32-C6 PlatformIO environments build successfully.

## Follow-up boundary

History, email and SMS activation remain administrator-controlled optional
services. Removing that remaining coordination belongs to the future onboarding
admin tool and does not reopen this firmware UX sprint.
