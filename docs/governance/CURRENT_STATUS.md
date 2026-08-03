# Current Status

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active beta preparation

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-02

## Purpose

This document records the current repository state. A capability is only called
validated when the repository contains corresponding evidence. Historical sprint
notes remain useful audit material, but are not by themselves proof of the current
revision.

## Current product direction

MOT is preparing a small ESP32-WROOM beta fleet. The active engineering focus is
SPR-0005: WROOM beta evidence and secure portal user/device onboarding proceed as
parallel workstreams. Onboarding is not planned for the firmware's local WebUI.

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
- WiFi operation is the currently dependable path.
- LTE/GPRS code exists, but the complete mobile MQTT/TLS path is not yet accepted
  as beta-ready.
- External L76K GPS support exists.

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

Implemented and validated in the controlled AWS development stack:

- server-side user-to-vehicle authorization for REST and WebSocket;

Not implemented:

- user/device registration and claim flow;
- ownership transfer and recovery;
- automated device certificate provisioning or rotation;
- cloud-managed OTA.

Authentication and assignment enforcement now exist in development. Two controlled
users passed exclusive-list, symmetric guessed-ID, live revoke/restore and expired
connection tests. Production hosting, self-service onboarding and the remaining
release/security gates are separate follow-up work.

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

The repository contains extensive historical validation notes and local ignored
PlatformIO build artifacts. During the 2026-07-31 Codex takeover audit, no build,
hardware test, deployment or cloud inspection was performed. Current head builds,
deployed AWS state and physical-device behaviour therefore remain to be
revalidated before beta release.

## Documentation status

DOC-001 completed a static, source-based documentation baseline on 2026-08-02.
Current navigation, canonical topic ownership, beta/support drafts, ADR/history
classification and the validation handover are present. Historical packages remain
retained and visibly separated; ambiguous rationale and destructive relocation wait
for ChatGPT Classic export reconciliation.

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
