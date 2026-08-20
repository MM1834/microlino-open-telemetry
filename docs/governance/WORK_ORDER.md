# Work Order

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-20

## High priority

### Complete Cognito SES sender rollout

**Active work package:** [SES-001 — Cognito Domain Email Delivery](../project/sprints/SES-001.md)

**Objective:** Send administrator-controlled Cognito account mail from the
DKIM-authenticated MOT domain with a monitored, reply-capable support identity.

**Current status:** SES identity, Hosttech DKIM publication, direct delivery and
AWS transactional production access pass. Cognito is deployed in `DEVELOPER`
mode using the verified MOT sender, and a renewed demo invitation was accepted
for delivery. Receipt and the first-password flow pass; validate password-recovery
delivery before closing the sprint.

### Correct the net-power History sign presentation

**Active portal sprint:** [HIS-SIGN-001 — Signed Net Power History](../project/sprints/HIS-SIGN-001.md)

**Objective:** Display consumption as negative and charging/regeneration as
positive without rewriting stored History or changing firmware/API semantics.

**Current status:** Repository implementation and responsive local acceptance
pass. `xrpioneer2` is already History-enabled and actively assigned to the
confirmed `xruser` account, so existing data is covered without migration. Hosted
desktop and smartphone acceptance after portal upload remains open.

### Complete unified C6 environment hardware gate

**Active work package:** [C6-ENV-001 — Unified C6 Build Environments](../project/sprints/C6-ENV-001.md)

**Objective:** Maintain one AWS-capable environment per C6 board and prove that
an unprovisioned device remains fully functional locally without AWS credentials.

**Current status:** Repository consolidation, both canonical builds, the XIAO
80.85% flash gate and 117 tests pass. The three deployed C6 devices are not
modified; physical unprovisioned acceptance waits for a spare board or the next
delivery.

### Execute HIS-001 bounded telemetry history pilot

**Active work package:** [HIS-001 — Bounded Telemetry History Pilot](../project/sprints/HIS-001.md)

**Objective:** Measure normal publish volume, then introduce a disabled-by-default,
31-day SOC/charging/plugged/speed history service with existing vehicle authorization,
fixed API ranges, portal charts and measurable AWS guardrails.

**Current status:** Repository implementation, local tests, AWS baseline
measurement and fail-closed deployment are complete. The controlled `pioneer`
and `xrpioneer` identities are enabled. Live, authorized portal and History
operation passed with fresh traffic. After road-test review, a repository update
now samples Speed and signed power per active minute, records immediate zero
transitions, suppresses continuous zero writes and averages both into the fixed
API resolutions. Speed reception gaps are closed at zero in the portal instead
of linearly interpolated. The AWS backend deployment and hosted portal upload
passed. The first moving-road-test/TTL/cost observation remains open. The existing
live state and WebSocket path remain the release boundary and must not regress.

A 2026-08-11 follow-up adds changed-odometer history and an
automatic personal range forecast with a fixed-SOC fallback and transparent
journey basis. Local tests, responsive visual checks and the controlled AWS
deployment pass. Hosted portal upload and live journey accumulation evidence
remain part of the open HIS-001 work.

### Execute JNY-001 journey summary and energy email pilot

**Active work package:** [JNY-001 — Journey Summary and Energy Email Pilot](../project/sprints/JNY-001.md)

**Objective:** Add an optional qualifying-journey email centered on consumed net
battery energy, while preserving explicit user consent, bounded AWS cost and
measured firmware resource margins.

**Current status:** The deployed preference API and hosted portal expose the
default-off `journeyEmailEnabled` opt-in. The repository backend now implements
a backward-compatible telemetry estimate, stable delayed journey completion,
idempotent email delivery and an additive firmware-counter priority path. The
backend and matching portal wording are deployed, and physical journey emails
with plausible telemetry-estimate values were received. The new hard
Standard-CAN charge boundary needs a repeat road observation. An independent
30-minute inactivity fallback for a missing terminal signal is also deployed;
its physical no-coverage garage observation remains open. Multi-journey energy
calibration and cost evidence remain open. Before firmware counters are
implemented, exact flash/RAM/heap impact must
still be measured on nanoESP32-C6-N16; XIAO fit remains non-blocking.

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
implementation remains open and is explicitly outside `v1.0.0-rc.1`. Retained legacy
ownership still uses the bounded compatibility guard until a reviewed migration
is required by fleet growth.

## Medium priority

### Simplify maintained firmware environments

Maintain one firmware line per board. The standalone GPS test environment is
retired and removed. Continue by stopping treatment of pre-AWS environments as
separate product firmware generations. Preserve AWS IoT as a normal configurable
feature while keeping local standalone operation.

### Revalidate the current repository revision

Run controlled offline builds first, followed by isolated static/backend tests,
read-only AWS inventory and explicitly approved hardware tests. Record evidence
against the exact commit and environment.

## Completed reference

- [NTF-FIX-001](../project/sprints/NTF-FIX-001.md) reconciled the stored
  notification email-confirmation state with SNS and passed hosted portal
  acceptance on 2026-08-20.
- [C6-SVC-001](../project/sprints/C6-SVC-001.md) completed shared ABRP and local
  onboarding parity, N16-AWS OTA/runtime acceptance and the strict XIAO 4 MB
  compatibility gate on 2026-08-11. XIAO vehicle/AWS hardware equivalence remains
  explicitly outside that completion claim.
- [WIFI-001](../project/sprints/WIFI-001.md) completed preferred Home-WiFi,
  Mobile fallback, automatic Home return and protected-AP recovery on C6.
- [NTF-001](../project/sprints/NTF-001.md) completed the bounded charging/SOC
  email and authenticated portal-settings pilot; SMS is deferred.
- [FW-SEC-001](../project/sprints/FW-SEC-001.md) completed the unique local
  administration, protected recovery and authenticated OTA boundary on WROOM and
  LilyGO. C6 carries its separately validated successor implementation.
- [C6-PH-001](../project/sprints/C6-PH-001.md) completed N16 local administration,
  recovery/OTA and runtime hardening plus XIAO compatibility validation on
  2026-08-07. Extended soak and production hardware qualification remain backlog.
- [C6-001](../project/sprints/C6-001.md) closed the bounded N16 dual-CAN
  WiFi/AWS pilot qualification on 2026-08-06.
- [WEB-001](../project/sprints/WEB-001.md) delivered the repository-owned public
  landing page; the maintainer deployed and validated it on desktop and smartphone
  on 2026-08-05.
- [`v1.0.0-rc.1`](../project/sprints/V1.0.0-RC.1.md) completed the repository and
  documentation consolidation and was published on 2026-08-04.
- REL-001 is complete and forms the validated predecessor baseline for
  [`v1.0.0-rc.1`](../project/sprints/V1.0.0-RC.1.md).
- DOC-001 is complete. Current owner pages now replace its intermediate migration
  and classification records.

## Parked reference

- [LTE-001](../project/sprints/LTE-001.md) preserves the functionally validated
  LilyGO A7670 WiFi/LTE baseline. Extended production qualification is parked;
  near-term firmware development is limited to the shared ESP32-C6 line.

## Completion policy

Work only moves to `CURRENT_STATUS` when it is present in the current code and has
appropriate validation evidence. Deferred opportunities move to
`ENGINEERING_BACKLOG`; historical implementation detail belongs in Git history.

## Related documents

- [CURRENT_STATUS.md](CURRENT_STATUS.md)
- [ENGINEERING_BACKLOG.md](ENGINEERING_BACKLOG.md)
- [SELF_REVIEW.md](SELF_REVIEW.md)
