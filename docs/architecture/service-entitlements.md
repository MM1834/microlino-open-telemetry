# Service entitlements and automatic activation

> **Status:** Proposed target architecture; not implemented
>
> **Audience:** Product owner, administrator, backend and portal developers
>
> **Last reviewed:** 2026-09-05

## Decision

MOT shall separate the commercial or administrative **entitlement** to use an
optional service from the user's **configuration and verification** of that
service. An administrator or later subscription system may grant a service before
an email address, mobile number or other destination exists. Delivery becomes
active automatically only after every service-specific prerequisite is satisfied.

This model is intentionally deferred until it can be implemented together with
the subscription model, the central administration portal and an isolated staging
environment. It must not be introduced as an interim mutation of the running
pilot notification records.

## Why the current pilot model is insufficient

The current service paths use different control mechanisms:

- History and offline cache Backfill are controlled by vehicle allowlists;
- the adapter-side History cache is a separate local setting;
- email is enabled by a per-user/per-vehicle preference and requires a confirmed
  destination;
- SMS additionally requires a verified destination and an administrative
  approval tied to the exact user, vehicle and destination fingerprint.

This is safe for a controlled pilot but cannot cleanly express an advance grant,
a subscription package or a reusable rule such as “activate SMS automatically
after the user verifies an eligible number”. In particular, active delivery must
never be simulated by inserting incomplete preference or destination records.

## State separation

Each optional service is evaluated from four independent facts:

| Dimension | Meaning |
|---|---|
| Entitlement | The administrator or subscription permits the service for the exact user and vehicle. |
| Configuration | The user has supplied the required settings, such as destination and thresholds. |
| Verification | The destination or prerequisite has passed its technical verification. |
| Runtime activation | All conditions are satisfied and the service may execute or deliver. |

The portal should expose user-facing states such as:

- `NOT_GRANTED` — the subscription does not include the service;
- `WAITING_FOR_CONFIGURATION` — granted, but required user settings are absent;
- `WAITING_FOR_VERIFICATION` — configured, but the destination is not confirmed;
- `ACTIVE` — granted, configured and verified;
- `SUSPENDED` — temporarily disabled by an administrator or safety control;
- `REVOKED` — entitlement removed; delivery stops immediately.

These are derived product states. The backend must retain the authoritative
entitlement, configuration and verification records separately rather than
allowing a portal client to write `ACTIVE` directly.

## Service activation matrix

| Service | Administrator/subscription grants | User or device completes | Automatic activation condition |
|---|---|---|---|
| Live telemetry | Base vehicle access | successful claim and device publication | active vehicle access and valid device identity |
| History | History entitlement | none | active vehicle access and History entitlement |
| Offline cache Backfill | Backfill entitlement | enable local cache on the adapter | active access, entitlement, accepted device policy and local cache enabled |
| Email notifications | Email entitlement | enter and confirm address; configure vehicle preferences | entitlement, confirmed address and enabled preference |
| Journey/charging/daily summaries | matching notification entitlement | select each desired report | parent email channel active and report preference enabled |
| SMS notifications | SMS entitlement and allowed country/tier | enter and verify number; enable vehicle preference | entitlement, verified eligible number, spend/rate controls and enabled preference |

The system may pre-grant any row. It must not send to an unverified destination.
Changing or deleting a destination immediately makes dependent services inactive
until the new destination is verified; an entitlement is not transferred to a
different user or vehicle.

## Entitlement model

The authoritative record should be keyed by stable Cognito `sub`, `vehicleId`
and a versioned service identifier, never by email address or telephone number.
A candidate record contains:

- `userSub`, `vehicleId`, `serviceId` and `schemaVersion`;
- `status` (`GRANTED`, `SUSPENDED` or `REVOKED`);
- optional subscription/package reference and plan version;
- `grantedAt`, `grantedBy`, optional `expiresAt` and reason code;
- optimistic version for conditional updates;
- audit correlation identifier.

Package names such as Basic, History, Notifications or Full are administrative
templates that expand into exact service entitlements. Runtime authorization
must evaluate the expanded service records, not trust a package label supplied
by the browser.

No entitlement record contains plaintext contact destinations, device private
keys, WiFi credentials or payment credentials. Billing and payment processing are
separate future concerns; an entitlement may reference an external subscription
identifier without making MOT the payment system of record.

## Backend and administration requirements

The future central administration API and portal shall:

- show effective service state per user and vehicle without exposing full contact
  destinations by default;
- grant, suspend, resume and revoke individual services or reviewed packages;
- preview every mutation and require an explicit apply step;
- use least-privilege authorization, conditional writes and idempotency keys;
- append a privacy-safe audit event for every administrative mutation;
- display prerequisite state separately: vehicle access, device readiness,
  destination configured, destination verified, local cache reported and runtime
  active;
- stop delivery immediately on vehicle deactivation, entitlement suspension or
  destination invalidation;
- keep country eligibility, sender identity, quotas, rate limits and spend alarms
  authoritative on the server.

The user portal remains responsible for destination entry, verification and
personal service preferences. Successful verification should trigger or be
followed by an idempotent entitlement evaluation so an already granted service
becomes active without another administrator action.

## Adapter-side cache boundary

Cloud authorization can accept offline Backfill in advance, but it cannot by
itself make the adapter record local data. The local cache remains an authenticated
device setting. A later cloud-managed configuration path may request that setting
only after its command authentication, acknowledgement, retry and recovery model
is reviewed. Until then, the administration portal must display the distinction
between “Backfill granted in cloud” and “cache enabled on adapter”.

## Required staging environment

The existing `/motbeta/` static portal is not a fully isolated test environment;
it shares important production-pilot backend resources. Before implementing this
model, create a staging environment with at least:

- a separate Cognito user pool and test users;
- separate entitlement, preference, destination, approval and audit tables;
- separate onboarding, vehicle and notification APIs/Lambdas;
- separate IoT test namespaces and non-production devices;
- an SMS/email test strategy that cannot contact real recipients accidentally;
- distinct portal configuration and callback/logout URLs;
- bounded budgets, alarms, logs and a reproducible teardown/reset procedure.

Tests must cover every state transition, retries, concurrent administrator/user
actions, destination replacement, entitlement expiry, vehicle deactivation,
cross-user isolation and fail-closed behavior when any dependency is unavailable.

## Migration and rollout

1. Build and validate the isolated staging environment.
2. Add the versioned entitlement store and read-only effective-state evaluation.
3. Integrate user verification events and preferences without changing current
   pilot delivery.
4. Add central administrator grant/suspend/revoke operations and audit.
5. Map existing History/Backfill allowlists and valid notification approvals to
   explicit entitlements using a previewed, idempotent migration.
6. Run both evaluations in shadow mode and compare results.
7. Enable automatic activation for selected test users, then a bounded pilot.
8. Retire the old per-service special cases only after rollback and delivery
   isolation tests pass.

Until this rollout begins, continue the current pilot procedure: administrators
may enable History and cloud Backfill; users configure and confirm email; SMS is
approved only after an eligible number has been entered and verified.

