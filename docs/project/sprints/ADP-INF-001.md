# ADP-INF-001 — MOT Adapter Information in Portal Settings

> **Status:** Complete — REV18 firmware-to-dashboard acceptance passed
>
> **Started:** 2026-09-07
>
> **Completed:** 2026-09-07

## Objective

Show the selected vehicle's current adapter identity, board, firmware, CAN
decoder assignments and local IP address at the end of the authenticated
Settings page. Reuse retained AWS State without adding a backend schema or
route.

## Scope and behaviour

- read `system/device_id`, `system/board`, `system/firmware_version`,
  `system/can1_profile`, `system/can2_profile` and `system/ip_address` from the
  existing authorized vehicle snapshot;
- reload the information when the user selects another assigned vehicle;
- keep the information read-only and separate from notification preferences;
- show `--` for topics not supplied by the selected firmware; REV18 supplies
  `system/board` together with firmware and decoder identities;
- retain German, English, French and Italian presentation;
- do not start dashboard telemetry polling or a WebSocket from Settings.

## Security and compatibility boundary

The existing Vehicle API JWT authorizer and active user/vehicle assignment remain
authoritative. The Settings page requests only the selected assigned vehicle's
snapshot through the existing provider. The displayed IP is informational and
may only be reachable while the browser is on the same local network. No secret,
credential, certificate or unrestricted diagnostic value is exposed.

## Acceptance

- [x] Adapter information appears after the editable Settings section.
- [x] Smartphone/tablet layout keeps adapter information after all editable settings.
- [x] DeviceId and Board are displayed when their retained topics are present.
- [x] Vehicle selection reloads preferences and adapter information together.
- [x] Missing retained values render as `--` without affecting Settings.
- [x] Snapshot failure is isolated from preference loading and saving.
- [x] Repository contract and JavaScript syntax tests pass.
- [x] Hosted portal acceptance with REV18 retained adapter identities.
- [x] Desktop and smartphone layouts accepted with populated current-firmware data.
- [x] Older firmware with empty adapter fields accepted using the `--` fallback.
- [x] Firmware, AWS State and dashboard presentation verified together.
- [x] Production `/dashboard/` acceptance reported by the maintainer on 2026-09-07.
