# ADM-UX-001 — Separate Portal Administration

> **Status:** Complete — hosted desktop/smartphone and role acceptance passed
>
> **Started:** 2026-09-03

## Objective

Move privileged beta-onboarding and Web-Flasher grant controls out of the normal
dashboard into a dedicated administration page. Keep the navigation entry and
the page functions unavailable to users without the existing administrator role.

## Authorization and navigation contract

- The dashboard renders the `Administration` navigation entry only for an
  authenticated access token containing Cognito group `mot-beta-admins`.
- Desktop uses the sidebar entry; the compact authenticated header exposes the
  same destination on smartphone layouts.
- `/dashboard/administration/` is a static route, but its privileged forms remain
  hidden until the local session is restored and the same group claim is present.
- Signed-out visitors and authenticated non-administrators see only a bounded
  access message and a route back to the dashboard.
- This client-side presentation is not an authorization boundary. The existing
  onboarding claim and firmware grant/revoke APIs continue to enforce their
  administrator authorization server-side.

## Scope

- move `Beta-Onboarding verwalten` to the dedicated page;
- move `Web-Flasher freigeben` and revoke to the dedicated page;
- preserve the one-time claim display/clear behavior;
- preserve target selection and bounded grant duration;
- retain German, English and French portal presentation;
- do not move the end-user firmware flasher, vehicle claim flow or notification
  settings.

## Acceptance

- [x] Privileged controls are absent from the dashboard document.
- [x] Administration links default hidden and require `mot-beta-admins`.
- [x] Direct page access hides all forms before and after a denied role check.
- [x] Existing server-side admin endpoints and bearer-token flow are retained.
- [x] Repository contract tests and JavaScript syntax checks pass.
- [x] Hosted desktop administrator acceptance.
- [x] Hosted smartphone administrator acceptance.
- [x] Confirm the menu and forms remain absent for a normal pilot user.

## Acceptance evidence

On 2026-09-03 the maintainer accepted the hosted page on desktop and smartphone
in German, English and French. An administrator account saw the navigation entry
and both administrative functions, while a standard user saw neither the menu
entry nor the privileged forms. Layout and return navigation passed on both
viewport classes. ADM-UX-001 is complete; commit and branch integration remain
separate repository operations.
