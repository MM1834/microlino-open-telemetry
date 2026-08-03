# Beta Screenshot Specification

> **Status:** Planned; capture only after UI and hardware validation
>
> **Audience:** Documentation maintainer and release reviewer

## Capture gate

Do not reuse historical screenshots as current evidence. Capture a new set only
after the exact firmware commit, hardware variant and tested workflow are recorded.
Portal onboarding images remain deferred until ONB-001 is implemented and reviewed.

## Test fixture

Use synthetic, non-personal values consistently:

- device label `MOT-DEMO01`;
- vehicle name `Beta Vehicle` and case-scoped vehicle ID;
- example WiFi/host values that disclose no real network;
- GPS coordinates hidden or replaced in documentation fixtures;
- no real passwords, tokens, certificate identifiers or account email.

Never populate a production credential merely to obtain a screenshot.

## Required local-WebUI set

| ID | View | Required state | Notes |
|---|---|---|---|
| WROOM-01 | Fallback network discovery | `MOT-<suffix>` visible | Crop unrelated nearby SSIDs |
| WROOM-02 | Wizard welcome/hardware | ESP32-WROOM, GPS/no-GPS variant | One capture per meaningful variant |
| WROOM-03 | Wizard network | Empty or synthetic fields | No real SSID/password |
| WROOM-04 | Wizard vehicle/CAN | Display-CAN selected | Make Standard-CAN limitation textual |
| WROOM-05 | Wizard services | Intended AWS/legacy/ABRP selections | Do not imply AWS connection proof |
| WROOM-06 | Wizard validation/finish | Flags visible | Caption that completion is not readiness proof |
| WROOM-07 | Status without GPS | Expected no-GPS state | Must not look like an error |
| WROOM-08 | Status with GPS | Detected and, separately, fix | Hide coordinates |
| WROOM-09 | Configuration/backup warning | Safe synthetic configuration | Explain export contains secrets |
| WROOM-10 | Local OTA | Password-protected access and page | Do not show password or legacy path as instruction |
| WROOM-11 | Recovery/fallback | Re-entry after failed WiFi | Validate before publishing exact timings |

System Health should be shown only if all host, IP, location and identifier fields
are demonstrably synthetic or permanently redacted in the source image.

## Viewports and metadata

Capture the essential workflow at:

- mobile portrait, approximately 390 CSS pixels wide;
- desktop, approximately 1440 CSS pixels wide.

Each accepted image record must include:

- screenshot ID and documentation page;
- firmware Git commit, reported version and PlatformIO environment;
- physical hardware/variant and validation record;
- browser, viewport and operating system;
- capture date, reviewer and redaction confirmation.

Store canonical images below `docs/assets/images/` and use descriptive stable names.
Do not add an image until its destination page and review owner are known.

## Portal backlog

After ONB-001, specify and validate portal captures for invitation/login, device
claim or assignment, authorization failures, vehicle list, device status, support
workflow and logout. Those images must show that a user sees only assigned vehicles;
mockups must be labelled as mockups rather than implementation evidence.
