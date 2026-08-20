# Current Status

**Project:** Microlino Open Telemetry (MOT)

**Status:** `v1.0.0-rc.1` published; REL-001 pilot deployed at `/dashboard/`

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-20

## Purpose

This document records the current repository state. A capability is only called
validated when the repository contains corresponding evidence. Historical sprint
notes remain useful audit material, but are not by themselves proof of the current
revision.

## Current product direction

HIS-SIGN-001 has a repository-complete portal refinement for historical net
power. It displays consumption as negative and charging/regeneration as positive,
using a symmetric axis and explicit zero line while preserving stored History and
API semantics. Existing `xrpioneer2` records need no migration; read-only AWS
checks confirmed that the vehicle is History-enabled and actively owned by the
confirmed `xruser` account. Local desktop and smartphone acceptance passed;
hosted upload and acceptance remain open.

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

AUTH-PERSIST-001 completed on 2026-08-14. The hosted portal now offers an
unchecked trusted-device opt-in that can renew its one-hour access session through
the existing 30-day Cognito refresh-token lifetime. Only refresh state is retained
persistently; the default remains browser-session-only, logout clears both storage
classes, and hosted desktop and smartphone acceptance passed.

NTF-FIX-001 completed on 2026-08-20. It corrects the stale portal email
confirmation flag by reconciling authenticated preference reads with the
authoritative SNS subscription state. The controlled `xrpioneer2` record changed
from false to true while SNS remained confirmed; stack and Lambda health checks
passed, followed by hosted portal acceptance after reload.

JNY-001 is active as a separate journey-summary and energy-email pilot. Its
default-off preference API is deployed and the first hosted checkbox is visible.
The deployed backend now adds stable delayed completion, an explicitly labelled
estimate from existing power telemetry, idempotent email delivery and automatic
priority for a future firmware counter. Its reviewed additive Change Set reached
`UPDATE_COMPLETE`; the operational portal text is hosted and the first physical
journey emails were received with plausible telemetry-estimate values.
Firmware-counter work is
still gated on measured flash, RAM and runtime-heap impact for the priority
nanoESP32-C6-N16 target; XIAO 4 MB fit is optional and non-blocking.

The first JNY-001 road observation identified and corrected a post-stop charging
edge: plugging in during the stability window no longer invalidates the completed
journey. That intermediate resume rule was later superseded by the authoritative
hard Standard-CAN charge boundary described below. Subsequent physical journey
email delivery passed.

JNY-001 now also retains the latest per-vehicle completion decision and exclusion
reason after active journey data is cleared. This diagnostic increment was
deployed in place on 2026-08-16 after the first legacy `xrpioneer2` observation
could not be explained retrospectively from the former cleared state. Successful
later completion clears an older exclusion reason.

Standard-CAN plug/charge is now treated as an authoritative hard JNY-001 journey
boundary rather than charging activity inside a drive. This prevents later
legacy speed noise from reopening a stopped journey, permits immediate finalizing
when charging begins and leaves subsequent real movement to start a new journey.
The in-place 2026-08-17 deployment passed its 33 local notification tests and AWS
runtime checks. Basic end-to-end journey email delivery is physically validated;
the hard charge-boundary variant still needs a repeat observation. Multi-journey
energy calibration, firmware-counter comparison and cost evidence remain open.

JNY-001 also has a deployed 30-minute inactivity fallback for journeys whose
terminal speed or charging signal cannot reach AWS because coverage disappears.
It closes the journey at the last relevant received signal, cancels the pending
offline marker if telemetry returns and explicitly labels timeout-based emails.
The 2026-08-18 in-place deployment passed 35 notification tests and AWS runtime
checks. A physical no-coverage garage observation remains open.

Future firmware feature development is now centered on the dual-CAN ESP32-C6
line, with the 16 MB nanoESP32-C6-N16 as the primary target. The 4 MB XIAO remains
a bounded compatibility target where the shared C6 implementation fits without a
fork. ESP32-WROOM and LilyGO remain supported at their validated baseline but are
no longer feature-development targets. WIFI-001 uses a second C6 WiFi profile for
an external LTE/GSM hotspot; replacement of LilyGO's onboard cellular path remains
an open hardware/architecture evaluation.

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

LTE-001 is parked as of 2026-08-09. Its validated WiFi/LTE baseline remains
supported, but the open extended soak, weak-signal, SIM-variation and repeated
failover qualification is not part of the active work order. FW-SEC-001 was
formally closed on the same date at its currently validated security baseline;
future firmware feature and security work is concentrated on the ESP32-C6 line.

## Firmware structure

The repository contains shared telemetry, CAN decoding, configuration, readiness,
MQTT topics, AWS IoT and GPS components under `firmware/common` and
`firmware/shared-libs`.

PlatformIO currently exposes these environments:

- `esp32dev`
- `esp32dev-aws`
- `lilygo-t-a7670`
- `T-A7670X-AWS`
- `nanoesp32c6-n16` — unified 16 MB C6 build with runtime-optional AWS;
- `xiao-esp32c6` — unified 4 MB C6 build with runtime-optional AWS.

C6-ENV-001 prepared this two-environment model after C6-SVC-001. Both C6 builds
include LittleFS and AWS IoT but remain local when `/aws` credentials are absent.
The repository gate can be completed without altering the deployed N16 and two
XIAO units; physical no-credential acceptance waits for a spare board or the next
delivery.

C6-001 closed on 2026-08-06 as a bounded pilot qualification. One shared C6 source
line and board-specific PlatformIO profiles provide two passive CAN inputs plus
optional GPS. The Muse Lab nanoESP32-C6-N16 is the recommended dual-CAN WiFi pilot:
it received both Pioneer CAN buses simultaneously, retained independent decoder
profiles and zero receive errors, and later delivered live AWS/portal telemetry.
The XIAO 4 MB profile compiled, flashed and passed GPS/startup checks but did not
receive the same vehicle dual-CAN/AWS qualification. C6-PH-001 completed the
protected setup/fallback AP, authenticated WebUI, WiFi/CAN/GPS/AWS diagnostics,
backup/restore, factory reset, shared local OTA core and cooperative runtime
reconnect. N16 base/AWS and WROOM regression builds pass. A physical N16 run on
2026-08-07 retained WiFi/AWS credentials, received both vehicle CAN buses with
zero errors, acquired a GPS fix and continued AWS publication. Protected fallback,
first setup, authenticated WebUI, secret-free backup/restore, successful local OTA
and invalid-image recovery also passed physically. A subsequent vehicle run
confirmed automatic hotspot/AWS return with fresh GPS telemetry. XIAO then passed
station/fallback WiFi, authenticated WebUI, GPS and physical administrator-password
recovery while preserving configuration. All C6, WROOM and LilyGO base/AWS builds
and 25 focused security tests passed. Extended soak and XIAO vehicle Dual-CAN/AWS
equivalence remain non-blocking follow-up evidence. Neither board is yet a generally
approved production module.

WIFI-001 now has a C6-only repository implementation for preferred Home-WiFi and
a second/mobile hotspot profile. The cooperative policy, authenticated WebUI,
serial configuration, secret-safe backup and diagnostics compile from the same
source for N16 and XIAO; 107 repository tests and all four C6 base/AWS builds pass.
The slice adds 5,222 bytes and leaves XIAO-AWS at 75.6% application flash. On
2026-08-08 XIAO physically passed Home/mobile fallback and automatic return while
GPS and both CAN loops continued. The test exposed and closed a stale
`WL_CONNECTED` with zero-IP edge case. On 2026-08-08 the N16-AWS image was
installed by OTA: it used the mobile hotspot outside Home coverage, continued
publishing, then automatically returned to Home WiFi on entering the garage and
updated its Home-network address in the portal. A USB-observed N16 road run then
captured Home loss, bounded fallback to Mobile, AWS reconnect, periodic Home
detection and automatic return. Both CAN counters advanced with zero controller
errors, live BMS data continued and GPS obtained a valid fix across the run. The
final controlled test disabled the only reachable hotspot: N16 started its
protected fallback AP after bounded retries while CAN/GPS continued, then recovered
to Mobile, AWS publication and automatically stopped the AP after the hotspot was
restored. WIFI-001 hardware and repository qualification is complete. The XIAO was then
explicitly erased, re-provisioned with the current 4 MB partition layout and an
empty valid LittleFS image, and verified in its clean protected-AP state for a
separate onboarding exercise.

The later REV5 N16 correction restores unconditional Home priority when its SSID
is visible, avoiding retention of a strong Mobile-hotspot WLAN whose cellular
uplink is unavailable. A real drive passed Mobile-to-Home return, AWS reconnect,
ABRP HTTP 200, error-free Dual-CAN and valid GPS; the focused 131-test suite and
both unified C6 builds pass.

REV6 adds one persistent, default-enabled GPS control across C6, WROOM and LilyGO.
Authenticated Configuration and Wizard surfaces offer it only after receiver
detection, while retaining the control when disabled. The C6-N16 OTA build passed
physical disable/re-enable acceptance on 2026-08-20, including `GPS_DISABLED`
runtime diagnostics and configuration export/import coverage. All four maintained
production build targets and the focused source contracts pass; the 4 MB WROOM
AWS image remains within its existing OTA application partition with minimal
remaining margin.

C6-SVC-001 completed on 2026-08-11. The shared C6 line now includes runtime-
selectable asynchronous ABRP and the authenticated seven-step local onboarding
wizard in the same AWS-capable image. A freshly built N16-AWS image was installed
through local OTA with configuration retained; the maintainer validated the local
wizard, status and successful ABRP HTTP path on the running device. Garage
conditions produced `GPS_DETECTED` without `GPS_FIX`, and the ABRP payload safely
omitted stale/unavailable coordinates. All four C6 environments, both WROOM
regression environments and 114 repository tests pass. The final XIAO-AWS OTA
binary uses 80.85% of its application slot, below the strict 85% gate; it was not
flashed and remains a compatibility target rather than an N16-equivalent hardware
qualification.

On 2026-08-09 the standard XIAO base/AWS profiles were corrected from an
unintended external-U.FL selection to the onboard ceramic antenna used by the
deployed modules. External U.FL selection remains an explicit per-build option
for hardware with a connected 2.4 GHz antenna. The N16 profiles are unaffected.

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

AUTH-PERSIST-001 adds an unchecked `Angemeldet bleiben` option to the repository
portal. The existing session-only PKCE path remains the default. An opted-in
trusted device persists only its refresh token and a maximum 30-day local expiry,
renews the one-hour access token through Cognito and clears both stores on logout
or permanent refresh rejection. Hosted desktop and smartphone close/reopen,
default-session regression and explicit logout acceptance passed on 2026-08-14.

NTF-001 has an additive notification pilot stack with separate preferences,
charging-session state and expiring event storage, an isolated IoT/Lambda path,
a JWT-protected preference API and filtered SNS email delivery. On 2026-08-07 a
physical `pioneer` charge from 58% across a temporary 60% target produced exactly
one stored event and one received email containing SOC and `vehicleId`; the target
was restored to 80%. SMS remains disabled because the AWS account does not yet
have End User Messaging SMS service access. The repository portal now contains a
first authenticated, per-vehicle settings UI for notification activation, target
SOC and email delivery. The hosted production portal successfully loaded the
stored `pioneer` settings and persisted several changed SOC targets after its
endpoint and exact CORS origin were corrected. The bounded email/portal pilot is
complete; SMS remains deferred.

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
  refinement is deployed and read back. The static portal package was uploaded on
  2026-08-09 and accepted on smartphone and desktop. GPS remains live-only.

On 2026-08-11 the controlled AWS stack also received changed-odometer history and
a bounded 30-day personal range forecast. The in-place update reached
`UPDATE_COMPLETE`; both updated Lambdas were active, Health returned 200, History
remained JWT-protected by 401 and the daily-write alarm remained `OK` at 2,100.
The matching shared `/dashboard/` and `/motbeta/` static source is repository-ready
but its hosted upload and live journey accumulation evidence remain open.

The hosted portal now uses a charging-first smartphone information hierarchy:
vehicle selection and compact connection indicators, charging state/power, SOC
and range, Speed, total mileage, location, detailed connectivity, engineering
cards, History, notifications, technical status and finally vehicle branding.
The same URL selects this layout responsively; desktop retains its established
dashboard layout. Plugged-but-not-charging is rendered consistently as
`Eingesteckt`, independent of telemetry arrival order.

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
because `pioneer` had not published after activation. Authenticated History,
live-path regression, motion enablement and the hosted static deployment have
since passed. The first moving-road observation and cost/TTL evidence remain open.
Cost Explorer access is not granted to the maintainer IAM user.

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
C6-PH-001 subsequently completed authenticated local administration, protected
setup/fallback AP, backup/reset, local OTA failure recovery and cooperative WiFi
reconnect. Extended soak, signed fleet rollback and production wiring/enclosure
approval remain deferred rather than implied by that result.

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
