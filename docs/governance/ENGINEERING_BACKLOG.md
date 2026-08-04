# Engineering Backlog

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-04

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

## Charging and SOC notifications

Add optional user notifications for charging milestones, initially by email and
later by web push or an application channel. Pilot examples include notifying the
driver when a configured SOC threshold is reached so a charging break can end, or
when an intended 80% charge limit has been reached.

The first implementation should consume normalized cloud state rather than add
firmware-specific notification logic. It must include:

- per-user and per-vehicle opt-in with a configurable SOC threshold;
- rising-threshold detection rather than one notification per telemetry update;
- at most one notification per threshold and charging session, with hysteresis or
  equivalent deduplication;
- delayed/missing telemetry handling and an explicit statement that this is an
  informational notification, not a vehicle charge-control function;
- privacy-safe delivery logs, unsubscribe controls and a bounded retention period;
- publish, Lambda and delivery-volume metrics with a small cost budget.

Email is the preferred pilot channel because it does not require an application or
browser push subscription. Provider choice, verified sender/domain handling and
production deliverability remain design decisions. Push is a later channel, not a
release dependency.

## Portal roles and vehicle sharing

After basic ownership enforcement, evaluate support/operator roles, multiple users
per vehicle, temporary sharing and least-privilege support access. Avoid encoding
vehicle ownership solely in Cognito attributes.

Concrete pilot request: allow an owner to grant a second existing portal user
read-only `VIEWER` access to one vehicle without creating a second owner. The
implementation must preserve the single-`OWNER` invariant, enforce roles in REST
and WebSocket paths, provide explicit grant/revoke operations and retain an audit
record. Manual duplicate ACTIVE assignments are not an accepted interim solution.

## Authentication session lifecycle

Add a reviewed refresh-token and reauthentication strategy. The current portal
clears expired sessions and requires a new login.

## Portal hosting migration

Before public self-registration, material pilot growth or a general-public portal
release, evaluate migration of only the static dashboard from shared hosting to a
controlled AWS static-hosting path, initially a private S3 origin behind
CloudFront. Retain the landing page and unrelated website content on the existing
hosting unless a separate need justifies moving them.

The objective is to remediate CLOUD-017 without purchasing a vServer subscription
solely for a low-volume pilot. The design must keep access logging disabled or
privacy-safe, preserve exact Cognito and API origins, include rollback and hosted
authorization regression tests, and operate under a small AWS budget alert. This
is a deferred option, not an active migration or a claim that the risk is already
resolved.

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

Reassess Device Shadows, ABRP over the shared LilyGO LTE/TLS transport and other
external services only after the beta identity, authorization and connectivity
foundations are reliable. SOC notification processing is tracked separately above
because it is a plausible bounded pilot feature.

## Related documents

- [CURRENT_STATUS.md](CURRENT_STATUS.md)
- [WORK_ORDER.md](WORK_ORDER.md)
- [SELF_REVIEW.md](SELF_REVIEW.md)
## Cloud cost controls

Grant a billing-only maintainer view or provide regular exported cost evidence,
create a small AWS Budget alert, and measure MQTT publishes per device. CloudWatch
observed roughly 2.78 million state-ingest Lambda invocations between 2026-07-01
and 2026-08-04. Evaluate batching/coarser state envelopes before fleet growth; do
not optimize the low-frequency onboarding claim path ahead of telemetry ingestion.
