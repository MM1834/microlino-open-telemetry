# Current Status

**Project:** Microlino Open Telemetry (MOT)

**Status:** REL-001 pilot release deployed at `/dashboard/`

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-04

## Purpose

This document records the current repository state. A capability is only called
validated when the repository contains corresponding evidence. Historical sprint
notes remain useful audit material, but are not by themselves proof of the current
revision.

## Current product direction

MOT has promoted the reviewed REL-001 repository pilot through `develop` to
`main`. WROOM evidence, secure portal onboarding and the LilyGO AWS/LTE functional
path are included. On 2026-08-04 the reviewed portal package was deployed to
`/dashboard/` after backup of the previous directory. The landing page at `/` and
the validated fallback portal at `/motbeta/` remain available. Hosted acceptance
passed for both users and all three devices. Onboarding is not planned for the
firmware's local WebUI.

The local WebUI remains the device-local setup, diagnostics, recovery and OTA
interface. The portal is the user-facing service for accounts, vehicle access and
future fleet functions.

## Supported hardware paths

### ESP32-WROOM

- Current reference platform for the beta devices.
- Supports CAN telemetry over WiFi.
- Can be deployed with or without an optional GPS module.
- Local WebUI, configuration, diagnostics and local OTA are implemented.
- AWS IoT support is implemented as a build option.

### LilyGO T-A7670G

- Shares the telemetry and AWS IoT architecture with ESP32-WROOM.
- WiFi remains the preferred transport and LTE/GPRS is the automatic fallback.
- AWS IoT X.509 over the A7670 TLS client was physically validated through the
  hosted portal on 2026-08-03 with WiFi unavailable.
- LTE resilience now uses explicit runtime APN configuration, bounded registration
  attempts, exponential reconnect backoff and modem recovery after repeated
  failures. Both LilyGO build variants and focused source tests passed on
  2026-08-04. The updated AWS build was then physically validated from WiFi loss
  through A7670 packet data and AWS IoT LTE/TLS reconnection without replacing its
  device identity or credentials, followed by a successful return to preferred
  WiFi and AWS IoT reconnection. Extended adverse-condition qualification remains
  open.
- External L76K GPS support exists.
- The operational setup AP uses the local administrator password, all operational
  WebUI/API routes require authentication and local OTA defaults to disabled.
- ABRP remains WiFi-only and was disabled for the validated AWS/LTE configuration.

## Firmware structure

The repository contains shared telemetry, CAN decoding, configuration, readiness,
MQTT topics, AWS IoT and GPS components under `firmware/common` and
`firmware/shared-libs`.

PlatformIO currently exposes these environments:

- `esp32dev`
- `esp32dev-aws`
- `esp32dev-gps-test`
- `lilygo-t-a7670`
- `T-A7670X-AWS`

This environment set is legacy structure, not the desired maintenance model. GPS
test firmware is no longer a maintained product variant. Pre-AWS environments are
also no longer intended as separate firmware generations. The target is one
maintained firmware line per board, with AWS IoT treated as a configurable feature.
That simplification is planned and has not yet been applied to PlatformIO.

## Portal and AWS backend

Implemented in the repository:

- static portal/dashboard;
- Cognito Authorization Code flow with PKCE;
- API Gateway JWT protection for vehicle REST routes;
- authenticated WebSocket connection;
- IoT Rule ingestion into DynamoDB;
- REST snapshots and live telemetry fan-out;
- per-device AWS IoT credentials loaded from LittleFS;
- shared AWS IoT firmware transport for both boards.
- HIS-001 pilot infrastructure for bounded SOC, charging, plugged and speed history,
  including TTL, authorized API and portal charts. The AWS development stack has
  SOC/charging and Speed enabled only for `pioneer`. Core and motion intervals
  are independently configurable and currently set to 5 and 15 minutes. GPS
  remains live-only.

The legacy Node-RED forwarder identity `xrpioneer` is provisioned with a dedicated
publish-only AWS IoT certificate, assigned to its controlled beta user and enabled
for the same bounded History service. The two-identity daily write alarm is 2,100
against a default-cadence maximum of 1,920 writes.

Implemented and validated in the controlled AWS development stack:

- server-side user-to-vehicle authorization for REST and WebSocket;
- atomic, expiring single-use vehicle claim flow;
- controlled administrator claim issuance;
- hosted Cognito login/logout and per-user vehicle isolation at `/dashboard/` and
  the retained `/motbeta/` fallback;
- exact HTTPS CORS for the canonical `www` portal origin.

Not implemented:

- ownership transfer and recovery;
- automated device certificate provisioning or rotation;
- cloud-managed OTA.
- public account self-registration.

HIS-001 was deployed on 2026-08-04 after measuring 1,761,194 ingest invocations
across 119 active hourly datapoints in the preceding week; steady active plateaus
were near 17,400/hour. The fail-closed deployment and subsequent one-vehicle core
activation both reached `UPDATE_COMPLETE`. The history table still had zero rows
because `pioneer` had not published after activation. Authenticated history,
live-path regression under fresh traffic, portal deployment, motion enablement and
cost/TTL observation remain open. Cost Explorer access is not granted to the
maintainer IAM user.

Authentication, assignment enforcement and controlled claiming now exist in the
development AWS account and hosted pilot portal. Two controlled users passed
exclusive-list, symmetric guessed-ID, live revoke/restore, expired-connection and
hosted role-separation tests. Public self-registration, lifecycle recovery and the
remaining release/security gates are separate follow-up work.

## Vehicle integration

The implemented firmware receives telemetry passively and does not control vehicle
behaviour. The current decoder and wiring are centered on the presently supported
Microlino CAN connection. Extending support to other vehicle models requires both
decoder work and a hardware decision for standard CAN access.

Known candidate paths are:

- rewire the current module from pins 1 and 9 to standard CAN;
- add another CAN interface/module;
- evaluate an ESP32-C6 generation design for the affected hardware variant.

No choice is currently approved.

## Validation status

Portal authorization and ONB-001.B2 have local, deployed-stack and hosted browser
evidence dated 2026-08-03. A previously unassigned no-GPS WROOM was erased,
provisioned with a unique AWS identity, claimed as `beta-02` by an existing user
and validated through the hosted portal. WROOM local security and AWS operation
were physically checked. On 2026-08-03 the LilyGO AWS build was hardened, flashed
without erasing its device identity, connected through LTE/TLS with WiFi absent and
delivered live CAN/GPS telemetry to the portal. REL-001 repository tests, firmware
compile gates, Git review gates and hosted acceptance are recorded in the release
validation evidence.

## Documentation status

DOC-001 completed a static, source-based documentation baseline on 2026-08-02.
Current navigation, canonical topic ownership, beta/support drafts, ADR/history
classification and the validation handover are present. Historical root packages
are retained under `docs/archive/` and visibly separated from current guidance.
Ambiguous rationale and destructive consolidation beyond fully preserved
duplicates still wait for ChatGPT Classic export reconciliation.

This documentation completion is not runtime evidence. Builds, hardware, deployed
AWS state, screenshots and beta release readiness remain to be validated.

## Security and local credentials

Local AWS IoT credential directories exist and are ignored by Git. Only `.gitkeep`
files are tracked. Ignore rules reduce accidental commits but do not provide
encryption, rotation or access control. Credential contents and deployed AWS
resources were not inspected during the takeover audit.

## Related documents

- [PROJECT_HANDOVER.md](PROJECT_HANDOVER.md)
- [WORK_ORDER.md](WORK_ORDER.md)
- [ENGINEERING_BACKLOG.md](ENGINEERING_BACKLOG.md)
- [SELF_REVIEW.md](SELF_REVIEW.md)
- [DOC-001 validation and handover](../project/DOC-001-VALIDATION.md)
