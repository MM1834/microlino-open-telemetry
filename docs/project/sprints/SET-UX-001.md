# SET-UX-001 — Separate Portal Settings

> **Status:** Complete — hosted `motbeta` desktop/smartphone acceptance passed
>
> **Started:** 2026-09-03

## Objective

Keep the primary portal focused on live vehicle information by moving personal,
vehicle-specific range and notification controls to a dedicated authenticated
settings page.

## Scope and behavior

- `/dashboard/settings/` owns range basis, SOC reserve, charge-target email,
  charging-stop email, journey and charging summaries, daily summary, email
  confirmation guidance and SMS verification/opt-in.
- The page restores the existing Cognito session, lists only vehicles assigned
  to that user and loads/saves preferences for the explicitly selected vehicle.
- It reuses the existing Vehicle and Notification APIs without starting portal
  telemetry polling or a WebSocket connection.
- The dashboard keeps a lightweight read of range basis and SOC reserve because
  those preferences directly affect its range forecast; no settings form remains
  embedded there.
- Desktop navigation points to the dedicated page. Smartphone navigation exposes
  a compact authenticated settings action. Administration links retain their
  existing `mot-beta-admins` presentation check.
- Existing stored settings and backend authorization contracts are unchanged.

## Acceptance

- [x] Dedicated settings page and return-to-dashboard navigation exist.
- [x] Main dashboard contains no notification/settings form.
- [x] Assigned-vehicle selection scopes every read and write.
- [x] Signed-out and vehicle-less access fail closed without exposing a form.
- [x] German, English and French presentation is retained.
- [x] Repository contract tests and JavaScript syntax checks pass.
- [x] Hosted `motbeta` desktop acceptance.
- [x] Hosted `motbeta` smartphone acceptance.
- [x] Changed values persist through the existing vehicle-specific API.

## Acceptance evidence

On 2026-09-03 the maintainer accepted the dedicated page in the hosted
`motbeta` path on desktop and smartphone and confirmed that changed values are
persisted. The same accepted package also retained the authorization-gated
Administration page, placed the end-user Web Flasher near the end of the normal
dashboard and used the photographic `microlino.jpeg` throughout. SET-UX-001 is
complete and approved for the normal `/dashboard/` release.
