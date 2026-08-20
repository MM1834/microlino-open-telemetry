# DEMO-001 — Static Portal Demo Dataset

> **Status:** Completed; deployed and hosted acceptance passed
>
> **Date:** 2026-08-20

## Objective

Provide a separate invited portal account with a frozen, anonymized copy of the
current `xrpioneer2` telemetry and its latest 30 days of History. The demo must
not expose the source vehicle, receive live telemetry or require a shared device
credential.

## Controlled identities

- source vehicle: `xrpioneer2` (read-only);
- demo vehicle: `demo-pioneer`;
- invited account: `demo@microlino-open-telemetry.ch`;
- fixed demo location: `47.46268167287872, 8.180829969601682`.

The email address is an invitation target, never an authorization key. Cognito
sends the invitation and the user sets the password. No password is accepted by
the refresh tool or stored in the repository.

## Data contract

- copy the most recent State values, but force the demo offline;
- remove source location/GPS, device ID/name, MQTT client ID, private IP,
  heartbeat and last-seen identity data;
- add exactly one fixed latitude/longitude State pair and no GPS History;
- copy only supported History signals from the latest 30-day source window and
  reduce them to the portal's actual server-side resolutions: five minutes for
  the last 24 hours, 30 minutes through day seven and two hours thereafter;
- preserve relative History spacing while shifting the newest sample to refresh
  time, and set a bounded 31-day TTL;
- replace only the exact `demo-pioneer` State and History partitions;
- never modify `xrpioneer2` or add `demo-pioneer` to live ingest allowlists.

## Operation

`tools/aws/refresh_demo_portal_data.py` is read-only by default. Its defaults are
the controlled identities and location above. Review the JSON plan, then repeat
with `--apply`. Re-running it deliberately refreshes the relative 30-day window;
the frozen dataset otherwise ages naturally and is not kept current by a
background service.

## Acceptance

- default execution makes no DynamoDB or Cognito mutation;
- target must use the `demo-` prefix and differ from the source;
- source State and History must both exist;
- focused unit tests cover sanitization, time shifting, location replacement,
  password-free Cognito invitation and the target guard;
- after apply, validate Cognito invitation, one ACTIVE assignment, REST isolation,
  30-day portal History and the single map point.

## Deployment evidence

The controlled 2026-08-20 apply copied 24 sanitized State records and 1,398
tiered History records to `demo-pioneer`. Read-only verification confirmed the
fixed latitude/longitude pair, `status/online=false`, one ACTIVE/OWNER assignment
with source `demo-001-static-copy`, and a new enabled Cognito identity in
`FORCE_CHANGE_PASSWORD`. No password was supplied to the tool. Six focused demo
tests and the existing ten controlled-onboarding tests pass.

The Cognito invitation was received, the user completed the first-password flow,
and the maintainer accepted the hosted demo presentation alongside normal user
accounts on 2026-08-20. The source vehicle is not modified by refresh.

## Read-only notification boundary

The first authenticated demo login exposed a separate abuse consideration:
although frozen telemetry cannot normally trigger SOC or journey mail, changing
the notification destination could ask SNS to send confirmation messages to
arbitrary addresses. The deployed Notification Preference API therefore treats
`demo-pioneer` as server-side read-only. GET returns only disabled preferences
with `readOnly=true`; PUT returns 403 `notifications_read_only` before any
DynamoDB write or `sns:Subscribe` call.

Runtime verification passed with the demo subject: GET returned 200/read-only,
an attempted arbitrary-email PUT returned 403, and the demo preference partition
remained empty. The hosted portal disables the controls and explains the demo
restriction. All 39 Notification/Journey tests pass.

## Read-only onboarding boundary

An ACTIVE assignment to `demo-pioneer` also makes claim consumption read-only.
The deployed onboarding Lambda checks the server-side access table before parsing
or reading a supplied claim and returns 403 `onboarding_read_only`. This prevents
the demo account from adding a real or other provisioned vehicle even if the
portal UI is bypassed. Administrator claim issuance is unchanged.

The reviewed Change Set modified only `OnboardingFunction`; an earlier candidate
that would have removed inherited stack tags was rejected before execution. Live
invocation with the demo subject returned the expected 403, the stack reached
`UPDATE_COMPLETE`, and 20 focused onboarding tests pass. The repository portal
hides **Fahrzeug hinzufügen** for `demo-*`. The maintainer accepted this hosted
presentation for the demo and confirmed that normal users remain unaffected.
