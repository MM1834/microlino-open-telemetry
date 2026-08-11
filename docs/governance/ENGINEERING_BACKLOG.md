# Engineering Backlog

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-09

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

## Notification follow-ups

The bounded email/portal work was completed in
[NTF-001](../project/sprints/NTF-001.md). Preserve the following later extensions
outside that completed cloud pilot:

- web push and application-native channels;
- direct SMTP or another notification path for modules intentionally operated
  without AWS;
- an optional GPIO target-SOC output for external local automation;
- fleet-wide policies and support/operator notification administration;
- automatic per-user cost allocation, chargeback, invoicing, tax handling and
  payment collection.

AWS continues to bill the project account. NTF-001 may retain provider message IDs,
delivery counts and actual SMS costs so later allocation remains possible, but it
must not imply that end-user billing is implemented.

## Portal user and administrator settings

Build a coherent authenticated portal settings area as user-configurable services
grow. NTF-001 is the first bounded self-service slice: each user manages notification
preferences and separately verified destinations for each authorized vehicle.

Later user settings may include account presentation preferences, time zone,
privacy/retention choices and other per-vehicle services. Define one versioned API
and storage model rather than adding unrelated settings to Cognito attributes or
static portal configuration.

Administrator functions must remain a separate least-privilege surface. A future
admin UI may expose inventory, invitation/claim state, roles, delivery health and
bounded support actions, but must not reveal device credentials or plaintext user
contact destinations by default. Every administrative mutation requires explicit
authorization and an audit record.

### Unified onboarding administration tool

Replace the current combination of AWS CLI procedures, the B1 helper and the
small portal claim form with one reviewed least-privilege administrator tool. It
may be a separate portal surface backed by dedicated administrator APIs, but must
not extend the firmware WebUI or expose AWS credentials to the browser.

Required capability slices:

- invite a Cognito user, show confirmation state and safely resend or cancel an
  unconfirmed invitation;
- display `sub`-based account identity without treating the email address as an
  authorization key;
- register and inspect the relationship between inventory `deviceId`, logical
  `vehicleId`, IoT Thing, certificate ID and firmware/hardware profile;
- show whether real telemetry state exists before enabling claim issuance, with a
  clear explanation for the current `vehicle_not_provisionable`/404 condition;
- issue, expire and revoke single-use claims while never logging or retaining the
  plaintext proof after its one-time display;
- show canonical ownership and access projections, detect legacy B1 records and
  prevent a second active `OWNER`;
- support reviewed email change on the same Cognito `sub` separately from a true
  new-account ownership transfer;
- implement B3 replacement, loss, recovery, retirement and ownership transfer
  with certificate rotation and resumable lifecycle checkpoints;
- provide a narrowly scoped test-data cleanup preview by topic class and time
  window, including separate State/History counts, explicit confirmation and a
  post-delete republish check;
- terminate revoked live sessions and run REST/WebSocket isolation checks;
- produce privacy-safe audit evidence and an exportable support summary without
  email addresses, tokens, claim proofs, private keys, WiFi secrets or telemetry
  values.

The tool must plan read-only by default, require an explicit apply step for every
mutation, use conditional writes, remain idempotent across retries and expose
effective AWS state rather than trusting stale stack parameters. Destructive
actions must resolve exact records first and must not offer an unrestricted
free-form DynamoDB deletion surface.

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

Hardware options previously requiring evaluation:

- rewire pins 1 and 9 of the current module to standard CAN;
- add another CAN transceiver/interface;
- use an ESP32-C6 generation board for the affected hardware variant. This option
  was promoted into active [C6-001](../project/sprints/C6-001.md) qualification on
  2026-08-05; it remains unapproved hardware until that sprint's gates pass.

The decision must consider electrical safety, isolation, connector compatibility,
available CAN controllers, firmware portability and support burden.

## ESP32-C6 production hardening

The local WebUI, protected AP, backup/reset, local OTA and cooperative reconnect
slice was completed in [C6-PH-001](../project/sprints/C6-PH-001.md). Production
hardware still requires controlled cross-channel
injection, simultaneous retained GPS/CAN/WiFi soak, stable regulated power,
termination review, strain-relieved wiring and enclosure qualification. Signed
rollback remains a later remote/fleet-OTA concern. The XIAO remains a 4 MB
compatibility target until it passes the same vehicle dual-CAN/AWS path.

[C6-SVC-001](../project/sprints/C6-SVC-001.md) completed shared ABRP and local
wizard parity with N16-AWS hardware acceptance. Base/non-AWS C6 environments
remain temporary regression and size-comparison gates until the AWS-capable image
is validated without provisioned AWS credentials; they are not separate product
firmware generations. The XIAO remains an unflashed 4 MB compatibility build at
80.85% of its OTA app slot and still lacks N16-equivalent vehicle/AWS qualification.

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

## LilyGO cellular-path replacement

Evaluate how the sustain-only LilyGO/A7670 path should eventually be replaced in
the C6-centered hardware direction. WIFI-001 covers only flexible use of an
external LTE/GSM WiFi hotspot as the immediate connectivity option; it does not
select a new integrated modem, carrier board or cellular transport architecture.

Any later replacement decision must compare external-hotspot operation with an
integrated modem path, including power control, antenna placement, reconnect and
coverage behaviour, TLS ownership, field recovery, enclosure/wiring burden and
long-term module availability. Do not create a second C6 firmware architecture
until hardware evidence justifies it.

## Parked LilyGO A7670 qualification

The functionally validated LilyGO WiFi/LTE path is retained as a sustain-only
baseline. LTE-001 already covers configured APN use, bounded reconnect/backoff,
modem recovery, AWS IoT X.509 fallback, return to preferred WiFi and successful
field recovery after coverage loss and a full power cycle.

Long-duration soak, controlled weak-signal thresholds, SIM removal/restore,
repeated transition cycles and removal of the remaining bounded WiFi wait are
parked. Do not resume this qualification or add LilyGO-only features without a
new priority decision. Near-term firmware feature work belongs in the shared C6
line; the existing LilyGO implementation remains available for maintenance and
regression fixes.

## Telemetry service groups and bundled state envelopes

Introduce two independent configuration dimensions before material C6/AWS pilot
growth:

1. **Transport services:** local-only, legacy MQTT, AWS IoT, ABRP and future
   consumers can be enabled independently. Existing WROOM service booleans are a
   partial board-specific implementation, not yet a shared cross-board model.
2. **Telemetry groups:** core/display, charging, BMS, location and diagnostics can
   be selected per transport with bounded intervals, change thresholds and
   immediate state-transition publication.

Replace the AWS scalar-topic burst with a versioned state envelope such as
`mot/<vehicleId>/telemetry/state/v1`. The current C6 full cycle can issue up to 18
individual telemetry publishes every five seconds, each causing a separate IoT
Rule invocation, ingest Lambda request, DynamoDB state write and possible live
fan-out. One sub-5-KB envelope at the same interval would reduce message/rule/
Lambda operations by up to roughly 18:1 for a complete C6 sample.

The optimization must be end-to-end. Merely unpacking one envelope into the same
number of DynamoDB writes would save IoT/Lambda requests but not state-write
volume. Prefer one current-state envelope item plus only the existing bounded,
selected history writes. Preserve immediate plug/charge/online transitions and
avoid waiting for the next periodic envelope.

Required migration work:

- versioned envelope schema with sequence, device timestamp, validity/age and
  optional groups;
- payload-size guard below one AWS IoT 5-KB metering unit;
- adaptive cadence for moving/charging versus parked/unchanged state;
- backend dual-read or bounded compatibility window for existing scalar devices;
- one WebSocket envelope fan-out and portal-side unpacking;
- retained-state, offline/stale-data and out-of-order semantics;
- publish, Rule, Lambda, DynamoDB and WebSocket counters before/after rollout;
- per-device fallback to scalar topics during migration, disabled after validation.

Do not enable both scalar and envelope publication indefinitely: that doubles
traffic and makes state precedence ambiguous. Decide this contract before the
first persistent C6 AWS pilot deployment if practical.

## Cloud cost controls

Grant a billing-only maintainer view or provide regular exported cost evidence,
create a small AWS Budget alert, and measure MQTT publishes per device. CloudWatch
observed roughly 2.78 million state-ingest Lambda invocations between 2026-07-01
and 2026-08-04. Evaluate batching/coarser state envelopes before fleet growth; do
not optimize the low-frequency onboarding claim path ahead of telemetry ingestion.

## Related documents

- [CURRENT_STATUS.md](CURRENT_STATUS.md)
- [WORK_ORDER.md](WORK_ORDER.md)
- [SELF_REVIEW.md](SELF_REVIEW.md)
