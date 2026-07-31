# Engineering Backlog

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Governance Version:** 1.0

**Last reviewed:** 2026-07-31

This backlog contains relevant work that is not part of the immediate active
delivery. Moving an item into `WORK_ORDER` requires an explicit priority decision.

## Cloud-managed firmware updates

Evaluate OTA distribution through the portal or AWS after beta onboarding is
stable. The firmware already supports local WebUI OTA, but remote OTA additionally
requires signed artifacts, integrity verification, rollout controls, rollback,
device status reporting and recovery procedures. AWS IoT Jobs is a candidate, not
an approved implementation.

## AWS IoT fleet operations

Design fleet provisioning, certificate rotation, revocation, device replacement
and ownership transfer. Continue to prohibit a shared operational certificate for
multiple deployed devices.

## Telemetry history and retention

Define durable cloud history storage, retention periods, export, deletion and cost
controls. Browser-local history and the current DynamoDB state snapshot are not a
complete fleet history service.

## Portal roles and vehicle sharing

After basic ownership enforcement, evaluate support/operator roles, multiple users
per vehicle, temporary sharing and least-privilege support access. Avoid encoding
vehicle ownership solely in Cognito attributes.

## Authentication session lifecycle

Add a reviewed refresh-token and reauthentication strategy. The current portal
clears expired sessions and requires a new login.

## Additional Microlino model decoders

Extend OBD/CAN decoding to additional vehicle models after obtaining verified
traces and choosing the required hardware interface. Preserve passive monitoring
and record signal confidence.

Hardware options requiring evaluation:

- rewire pins 1 and 9 of the current module to standard CAN;
- add another CAN transceiver/interface;
- use an ESP32-C6 generation board for the affected hardware variant.

The decision must consider electrical safety, isolation, connector compatibility,
available CAN controllers, firmware portability and support burden.

## Decoder profiles and compatibility data

Create a versioned vehicle-profile model with traceable signal evidence, firmware
compatibility and safe fallback behaviour for unknown models.

## Beta support tooling

Evaluate privacy-conscious support bundles, device health summaries and audit-safe
diagnostics. Support tooling must redact WiFi passwords, tokens, certificates and
private keys by default.

## Automated verification

Introduce CI for documentation links, configuration/schema tests, firmware compile
checks and isolated tests for inline backend handlers. Hardware-in-the-loop tests
may be added later where they provide repeatable value.

## Cloud infrastructure modularization

Consider extracting inline Lambda code from the monolithic CloudFormation template
when the backend begins changing frequently. Keep infrastructure and deployable
code versioned together.

## Local credential protection

Move beyond ignore-only storage toward an explicit encrypted or OS-protected local
workflow once beta onboarding no longer depends on the temporary credential
directories.

## Future transports and integrations

Reassess Device Shadows, ABRP over LTE, notification processing and other external
services only after the beta identity, authorization and connectivity foundations
are reliable.

## Related documents

- [CURRENT_STATUS.md](CURRENT_STATUS.md)
- [WORK_ORDER.md](WORK_ORDER.md)
- [SELF_REVIEW.md](SELF_REVIEW.md)
