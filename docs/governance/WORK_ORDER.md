# Work Order

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-02

## High priority

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

**Current status:** Authentication and telemetry APIs exist. Ownership enforcement,
registration and claiming do not.

**Current workstream:** ONB-001 within SPR-0005.

## Medium priority

### Simplify maintained firmware environments

Maintain one firmware line per board. Retire the GPS test environment and stop
treating pre-AWS environments as separate product firmware generations. Preserve
AWS IoT as a normal configurable feature while keeping local standalone operation.

### Stabilize LilyGO LTE/GPRS

Bring the LilyGO mobile transport to the same operational standard as its WiFi
path. Validate modem ownership, TLS, time, reconnect/backoff, watchdog behaviour,
power conditions and long-running telemetry before declaring it beta-ready.

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
