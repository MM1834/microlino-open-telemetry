# Current Status

**Project:** Microlino Open Telemetry (MOT)

**Status:** `v1.0.0-rc.1` published; REL-001 pilot deployed at `/dashboard/`

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-06

## Purpose

This document records the current repository state. A capability is only called
validated when the repository contains corresponding evidence. Historical sprint
notes remain useful audit material, but are not by themselves proof of the current
revision.

## Current product direction

MOT has promoted the reviewed REL-001 repository pilot through `develop` to
`main` and opened the consolidated `v1.0.0-rc.1` release candidate. WROOM evidence,
secure portal onboarding and the LilyGO AWS/LTE functional
path are included. On 2026-08-04 the reviewed portal package was deployed to
`/dashboard/` after backup of the previous directory. The landing page at `/` and
the validated fallback portal at `/motbeta/` remain available. Hosted acceptance
passed for both users and all three devices. Onboarding is not planned for the
firmware's local WebUI.

The local WebUI remains the device-local setup, diagnostics, recovery and OTA
interface. The portal is the user-facing service for accounts, vehicle access and
future fleet functions.

The repository-owned public landing page under `build/landing/current/` was
deployed to the hosted root by the maintainer on 2026-08-05 and successfully
tested on desktop and smartphone. It presents the validated architecture, portal,
controlled onboarding, direct ABRP path and dated project status while remaining
operationally separate from `/dashboard/` and `/motbeta/`.

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
  WiFi and AWS IoT reconnection. A subsequent maintainer road test confirmed LTE
  fallback while driving, recovery after loss of mobile coverage and correct
  operation after a full power cycle. ABRP remains WiFi-only. Extended soak and
  controlled adverse-condition qualification remain open.
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
- `lilygo-t-a7670`
- `T-A7670X-AWS`
- `nanoesp32c6-n16` — C6-001 qualification build;
- `xiao-esp32c6` — C6-001 compatibility qualification build;
- `nanoesp32c6-n16-aws` — N16 WiFi/AWS pilot build;
- `xiao-esp32c6-aws` — XIAO WiFi/AWS compatibility build.

C6-001 closed on 2026-08-06 as a bounded pilot qualification. One shared C6 source
line and board-specific PlatformIO profiles provide two passive CAN inputs plus
optional GPS. The Muse Lab nanoESP32-C6-N16 is the recommended dual-CAN WiFi pilot:
it received both Pioneer CAN buses simultaneously, retained independent decoder
profiles and zero receive errors, and later delivered live AWS/portal telemetry.
The XIAO 4 MB profile compiled, flashed and passed GPS/startup checks but did not
receive the same vehicle dual-CAN/AWS qualification. Neither board is a generally
approved production module; the deferred hardening gates are recorded separately.

On 2026-08-06 controlled lower-SOC charging confirmed Pioneer Standard-CAN pack
current at 0.3 A per raw unit, pack voltage, derived power and stable plug/charge
state. The shared decoder now applies bounded plausibility gates. Common AWS BMS
topics, C6 WiFi/AWS build profiles and portal live rendering are implemented and
compile/contract tested in the repository. The later N16 road run physically
validated WiFi/TLS publication and live state ingestion.

Three subsequent flat-road dual-CAN captures confirmed the same current scale and
sign during traction and regeneration. Negative pack current is discharge;
positive pack current is charging or regeneration. Light braking increased
observed electrical regeneration from about 4.5 kW on pedal release to about
7.8 kW. Firmware now publishes explicit battery- and vehicle-sign power values
plus regeneration/discharge states, and the repository portal renders the
vehicle convention. Deployment of these additions remains open.

The N16 qualification device was subsequently flashed with the AWS profile and
received a unique per-device AWS IoT Thing/certificate for vehicle `pioneer`.
LittleFS upload passed and firmware loaded the credentials. At that checkpoint
WiFi was not yet configured, so no TLS or publication evidence was claimed.

A subsequent approximately 12.7-minute home drive then physically validated the
N16 WiFi/AWS path through live state ingestion: both CAN channels remained
error-free, AWS received the new BMS and energy-flow suffixes, and the device
remained online. The run exposed a valid approximately 20.9 kW discharge peak
above the original 15 kW symmetric filter. The decoder now uses a 25 kW discharge
and 12 kW charge/regeneration boundary; the corrected AWS image was reflashed and
reconnected with credentials and WiFi preserved. Hosted static-portal deployment
and extended coexistence/soak remain open.

The standalone GPS test firmware and its PlatformIO environment have been removed.
Pre-AWS environments are no longer intended as separate firmware generations but
remain as legacy regression paths. The target is one maintained firmware line per
board, with AWS IoT and optional GPS treated as configurable capabilities. The
remaining environment simplification is still planned.

## Portal and AWS backend

Implemented in the repository under the consolidated `build/dashboard/current/`
portal source tree:

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
  SOC/charging and Speed enabled for the controlled `pioneer` and `xrpioneer`
  identities. A repository update changes Speed from a 15-minute snapshot to
  one-minute moving samples, suppresses repeated standstill zeroes, writes a
  journey-end zero immediately and averages Speed at API resolution. Signed power
  now uses the same active-minute aggregation, and the portal closes Speed/power
  reception gaps at zero instead of drawing diagonal phantom values. The backend
  refinement is deployed and read back; the static hosted portal package awaits
  manual FTPS upload. GPS remains live-only.

The legacy Node-RED forwarder identity `xrpioneer` is provisioned with a dedicated
publish-only AWS IoT certificate, assigned to its controlled beta user and enabled
for the same bounded History service. On 2026-08-05 its unstable second MQTT
subscriber was replaced with exact allowlisted `ioBroker in` state subscriptions;
successive AWS reads confirmed continuously changing SOC, Speed and power values.
The two-identity daily write alarm remains 2,100.

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

Known candidate paths were:

- rewire the current module from pins 1 and 9 to standard CAN;
- add another CAN interface/module;
- evaluate an ESP32-C6 generation design for the affected hardware variant.

C6-001 selected the nanoESP32-C6-N16 plus two external transceivers as the bounded
dual-CAN WiFi pilot path. Pilot duplication must retain the recorded GPIO mapping,
USB power, per-device credentials and one-active-publisher-per-vehicle rule.
Authenticated C6 local administration, runtime OTA/rollback, extended soak and
production wiring/enclosure approval remain deferred rather than implied by the
pilot result.

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
The `v1.0.0-rc.1` consolidation removes superseded README, SPR, manifest, patch and
hotfix packages from the current working tree after mapping current owners. Git
history retains the removed records. Current navigation, canonical topic ownership,
beta/support guidance and exact release evidence remain visible without an in-tree
historical archive.

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
- [v1.0.0-rc.1 consolidation sprint](../project/sprints/V1.0.0-RC.1.md)
