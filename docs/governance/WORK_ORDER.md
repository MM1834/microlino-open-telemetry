# Work Order

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-04

## High priority

### Harden local firmware administration — active

**Active security sprint:** [FW-SEC-001 — Local Firmware Administration Hardening](../project/sprints/FW-SEC-001.md)

**Objective:** Require a unique local device password, protect recovery AP and
sensitive WebUI/OTA operations, and close secret-echo paths before hardware is
issued to external pilot users. ESP32-WROOM and LilyGO now implement and physically
pass the same local-administration boundary.

### Execute SPR-0005 beta readiness and portal onboarding

**Active sprint:** [SPR-0005 — ESP32-WROOM Beta Readiness and Portal Onboarding](../project/sprints/SPR-0005.md)

**Execution model:** Credential safety and WROOM build/device evidence run in
parallel with ONB-001 authorization/onboarding. Both lanes converge at the beta
release gate; neither substitutes for the other.

### Prepare the ESP32-WROOM beta release

**Objective:** Deliver a small number of ESP32-WROOM devices, with or without GPS,
to beta testers with the essential setup, diagnostics, recovery and support
documentation.

**Dependencies:** Reproducible firmware builds, supported firmware-environment
definition, device provisioning procedure, security review and a beta support
runbook.

**Expected outcome:** Individually identifiable devices can be provisioned,
recovered and supported without sharing credentials.

**Current workstream:** SPR-0005.A through SPR-0005.D.

### Implement portal user and device onboarding

**Objective:** Add secure account onboarding, device claiming and per-user vehicle
authorization to the portal website and backend.

**Scope boundary:** This is a portal/backend capability. It must not be implemented
as an Internet-facing extension of the firmware's local WebUI.

**Required design work:**

- canonical identity model for user, vehicle, device, Thing and certificate;
- server-side `UserVehicleAccess` or equivalent authorization store;
- authorization on REST list/snapshot routes and WebSocket subscriptions;
- invitation or controlled beta account creation;
- one-time device claim/bootstrap mechanism;
- recovery, replacement, ownership transfer and revocation;
- audit events and support-safe diagnostics.

**Current status:** Authentication, telemetry APIs, controlled ownership and the
B2 claim flow are deployed and two-user validated through the hosted pilot portal.
Transfer/replacement, public registration and production release controls remain
open.

**Current workstream:** ONB-001 within SPR-0005.

**Active slice:** ONB-001.B2 is deployed and functionally validated. B3 lifecycle
implementation remains open and is explicitly outside REL-001. Retained legacy
ownership still uses the bounded compatibility guard until a reviewed migration
is required by fleet growth.

## Medium priority

### Simplify maintained firmware environments

Maintain one firmware line per board. Retire the GPS test environment and stop
treating pre-AWS environments as separate product firmware generations. Preserve
AWS IoT as a normal configurable feature while keeping local standalone operation.

### Qualify LilyGO LTE/GPRS beyond the functional pilot path

AWS IoT X.509 over LTE/TLS, WiFi preference/fallback and live CAN-to-portal data
are functionally validated. Continue with long-running soak, weak-signal, modem
recovery, watchdog and power-condition testing before declaring the path generally
production-ready. ABRP remains WiFi-only.

### Revalidate the current repository revision

Run controlled offline builds first, followed by isolated static/backend tests,
read-only AWS inventory and explicitly approved hardware tests. Record evidence
against the exact commit and environment.

## Completed reference

- [REL-001 portal pilot release](../project/sprints/REL-001.md) is complete; its
  release notes, validation evidence and risk decisions remain authoritative.
- [DOC-001 documentation baseline](../project/sprints/DOC-001.md) is complete;
  historical material is separated from current guidance.

## Completion policy

Work only moves to `CURRENT_STATUS` when it is present in the current code and has
appropriate validation evidence. Deferred opportunities move to
`ENGINEERING_BACKLOG`; historical implementation detail belongs under legacy or
release documentation.

## Related documents

- [CURRENT_STATUS.md](CURRENT_STATUS.md)
- [ENGINEERING_BACKLOG.md](ENGINEERING_BACKLOG.md)
- [SELF_REVIEW.md](SELF_REVIEW.md)
