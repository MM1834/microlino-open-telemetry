# Work Order

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-04

## High priority

### Promote the hosted portal pilot — complete

**Release sprint:** [REL-001 — Portal Pilot Release Readiness](../project/sprints/REL-001.md)

**Current result:** The reviewed implementation and evidence are merged through
`develop` into `main`. The controlled package was deployed to `/dashboard/` on
2026-08-04 after backup of the previous directory. Both users and all three devices
passed the hosted acceptance checks across `/`, `/dashboard/` and the retained
`/motbeta/` fallback. `/` remains the landing page.

**User terminology:** Early externally supported accounts are pilot users, not a
separate permanent beta-user class. Their accounts and assignments may continue
unchanged into the regular release.

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

### Consolidate repository documentation — complete

**Objective:** Establish one current documentation generation and preserve older
material as explicitly historical evidence.

**Completed sprint:** [DOC-001 — Documentation Consolidation and Beta Baseline](../project/sprints/DOC-001.md)

**Current status:** The static baseline, beta/support drafts, history/ADR
classification and [validation handover](../project/DOC-001-VALIDATION.md) are
complete. Runtime evidence and maintainer release approval remain separate gates.

**Expected outcome:** Maintainers and beta users can distinguish current product
behaviour, planned work and historical implementation records.

**Review boundary:** Historically ambiguous decisions and validation claims are
preserved for reconciliation with the future ChatGPT Classic export.

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

## Completion policy

Work only moves to `CURRENT_STATUS` when it is present in the current code and has
appropriate validation evidence. Deferred opportunities move to
`ENGINEERING_BACKLOG`; historical implementation detail belongs under legacy or
release documentation.

## Related documents

- [CURRENT_STATUS.md](CURRENT_STATUS.md)
- [ENGINEERING_BACKLOG.md](ENGINEERING_BACKLOG.md)
- [SELF_REVIEW.md](SELF_REVIEW.md)
