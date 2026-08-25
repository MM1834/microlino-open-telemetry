# Engineering Backlog

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-08-20

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

## GPS-derived UTC for offline History cache

Validate and harden the existing GPS system-time path as an optional independent
UTC source for the SOC/Speed offline cache. `MotGps` already accepts valid GNSS
date/time, calls `settimeofday()` and thereby feeds the same system clock consumed
by the cache. GPS remains recommended rather than required: NTP and uninterrupted-
power operation are valid for no-GPS adapters such as B025.

The Pioneer offline cold-start proof passes: GNSS set system UTC before any
network connection and the resulting SOC/Speed cache replay completed. Remaining
hardening requires rejection of stale or implausible receiver time plus
deterministic precedence and drift handling between
NTP and GPS, and cold-start/outage tests. The cache interface may receive only
validated UTC and validity metadata. Coordinates, routes and raw location fixes
must never enter the local journal or History Backfill envelope.

## Notification follow-ups

The bounded email/portal work was completed in
[NTF-001](../project/sprints/NTF-001.md). Preserve the following later extensions
outside that completed cloud pilot:

- charging-stop-before-target detection and controlled SMS rollout are now active
  in [NTF-002](../project/sprints/NTF-002.md);
- observe whether the deployed 45-second continuous-charging qualification is
  sufficient across vehicles and chargers. The first real startup oscillation
  justified the gate; collect further traces before changing its duration;
- retain a hard server-side rate boundary for any later SMS rollout. Ladestopp is
  limited to one delivery per user, vehicle and plugged session; SMS additionally
  needs spend limits and alarms before activation;
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

### Pilot handoff and vehicle presentation

Turn the controlled beta onboarding procedure into a short user-facing handoff
flow while preserving the administrator security boundary. The pilot package
should include a one-page setup guide, a protected support route and an explicit
handoff checklist for local AP setup, administrator password, WiFi, CAN1 profile,
first AWS publication, account invitation, claim consumption, History enablement
and notification opt-in.

The handoff must spell out **AP** once as **WLAN/WiFi Access Point** and give the
non-technical local connection sequence explicitly: connect a laptop, smartphone
or tablet to `MOT-xxxx` using the protected inventory password, open
`http://192.168.4.1`, then authenticate as `setup` with that password. Account
invitation may run before or in parallel with device preparation; the workflow
must present an account without a vehicle as a valid intermediate state.

### Unified local setup and first-run wizard

**Completed (2026-08-25):** Implemented for both C6 targets and accepted through
repeated factory-first-run testing on B025 plus configuration-preserving firmware
updates on B021, B023 and B024. The reproducible onboarding guide and one-page
pilot handout describe the resulting workflow. See
[ONB-UX-001](../project/sprints/ONB-UX-001.md).

Harden the C6 local first-run journey around one coherent WebUI shell. Today the
one-time `setup` page establishes the `admin` credential, while the wizard links
to a separate full configuration form. Saving WiFi there reboots the device; a
successful station connection then stops the fallback AP and removes the known
`192.168.4.1` route before the user has completed the wizard. Reconnecting later
also changes the required username from `setup` to `admin` without enough
in-product guidance. The implementation works technically but is not suitable as
a standard-user handoff.

Required behavior:

- keep `setup` as a one-time credential transition and explain before reboot that
  all subsequent authentication uses `admin` plus the newly chosen password;
- render first-run steps and later configuration from the same settings sections
  and save handlers, rather than sending the user into a disconnected initial
  configuration screen;
- automatically launch the wizard only while onboarding is incomplete, persist
  the last completed step across every required reboot and resume there;
- keep the WPA2-protected AP and `192.168.4.1` available while onboarding remains
  incomplete, even when Home or Mobile WiFi connects; stop that extra AP after
  explicit completion under the normal bounded stability rule;
- explain on the WiFi step that the protected `MOT-xxxx` hotspot remains active
  during onboarding, normally becomes inactive after completion when Home or
  Mobile/WiFi2 is connected, and automatically returns when neither configured
  station network is reachable;
- after every network-changing save, show what will reboot, which credential to
  use, and all available reconnection routes without displaying a secret;
- show a dynamic handoff notice on the final wizard page. When the station is
  connected, identify the active profile as Home or Mobile/WiFi2, display its
  escaped SSID and current local IP address, and state that future access normally
  uses that network. Never render a configured-but-inactive SSID as the current
  connection and never display either WiFi password;
- on that same page, explain the fallback: if neither Home nor Mobile/WiFi2 is
  available, the protected `MOT-xxxx` hotspot becomes active and local
  configuration is reachable again at `http://192.168.4.1` using the `admin`
  credential;
- after completion, route `/` to normal status/configuration and offer an
  explicit, authenticated **Run setup wizard again** support action without
  reverting the local administrator credential.

Acceptance must cover first Home WiFi, first Mobile/WiFi2 profile, an unavailable
configured network, station success during onboarding, reboot/resume at every
network-changing step, explicit completion and a later configuration change. It
must confirm that the temporary AP extension ends after completion and does not
weaken the existing authentication or same-origin mutation checks. UI acceptance
must additionally verify correct active-profile/SSID/IP display, the offline
fallback wording and suppression of all stored network secrets.

Add a user-editable portal display name or alias for each authorized vehicle.
Changing it must not alter `vehicleId`, Thing name, device identity, certificate
scope or authorization records. Until this exists, keep the readable name in the
protected inventory and show the stable `vehicleId` where ambiguity matters.

Improve onboarding feedback so an administrator can distinguish, without exposing
telemetry values, between an unknown inventory record, missing first publication,
an existing owner and a claim that is ready to issue. Review the generic Cognito
invitation/verification mail text and brand it only through supported, maintained
configuration rather than instructions that could weaken account verification.

Define a repeatable acceptance-test cleanup for temporary pilot accounts and test
adapters. It must preview and separately handle Cognito identity, ownership/access,
claims, notification subscriptions/preferences, History, live State, inventory,
Thing/certificate and local device configuration. A factory reset alone must never
be described as cloud-account or ownership cleanup.

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

The definitive workflow must remove routine user–MOT ping-pong: after the
administrator has prepared inventory and policy approvals, the tool guides the
user through account confirmation, local device setup, claim consumption and
optional notification verification without requiring a separate administrator
message between ordinary steps. It must track prerequisites independently so
account activation can precede device onboarding and History, email or SMS can
be enabled later without restarting the process. Only exceptional, security-
relevant or failed states are handed to an administrator.

The tool must plan read-only by default, require an explicit apply step for every
mutation, use conditional writes, remain idempotent across retries and expose
effective AWS state rather than trusting stale stack parameters. Destructive
actions must resolve exact records first and must not offer an unrestricted
free-form DynamoDB deletion surface.

### User-controlled vehicle deactivation and deletion lifecycle

Add an authenticated owner workflow that clearly separates reversible visibility
from authorization revocation and irreversible data deletion. The current portal
does not let a user deactivate, unlink or delete a vehicle; controlled beta
operators can only revoke an access projection. A `REVOKED` assignment removes the
vehicle from the normal list but does not delete ownership, telemetry, History,
notification settings, the IoT Thing or device credentials.

Required product semantics:

- **Hide:** a per-user presentation preference that removes a vehicle from the
  default selector without changing ownership, live access or device ingestion;
- **Deactivate/unlink:** an owner-confirmed, reversible lifecycle transition that
  removes REST/WebSocket access and notification delivery, terminates live
  sessions and defines whether the device may continue ingesting telemetry;
- **Delete data:** a separately confirmed request with an explicit preview for
  current state, History, locations, notification preferences/events and audit
  retention; never infer this from hiding, logout, factory reset or unlinking;
- **Retire device/vehicle:** an administrator-reviewed B3 operation that deactivates
  certificates, preserves required audit evidence and prevents silent identity
  reuse;
- **Restore:** a conditional, audited reactivation path that rejects a new owner,
  retired identity or unresolved transfer instead of overwriting lifecycle state.

The portal must explain consequences and retention before confirmation, require
recent authentication for destructive requests, use generic non-enumerating
errors, and remain safe under retries. Ownership, access, device ingestion,
History allowlisting and notification delivery must use one coherent lifecycle
contract so a vehicle cannot disappear from the selector while continuing
unexpected emails or remain visible through an existing WebSocket connection.

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
wizard parity with N16-AWS hardware acceptance. [C6-ENV-001](../project/sprints/C6-ENV-001.md)
now removes the duplicate C6 non-AWS/AWS environment pairs while retaining AWS as
a runtime-optional capability. Physical operation without provisioned credentials
remains the rollout gate because all three current C6 boards are deployed. The
XIAO remains a 4 MB compatibility target and still lacks N16-equivalent vehicle/AWS
qualification.

### Pilot-adapter power and status indication

Evaluate a low-current indicator for the next ESP32-C6 adapter carrier or custom
PCB. A simple power LED may be connected to the regulated supply through a
dimensioned series resistor and therefore does not require a GPIO. If field
diagnostics justify a controllable status LED, reserve and validate a suitable
GPIO separately without conflicting with dual CAN, GPS, USB boot/diagnostics or
future peripheral assignments. Measure the standby-current impact and define the
LED meanings before adding it to unattended pilot hardware.

## ESP32-C6 operating power modes

**Priority:** High. Define and qualify an explicit energy-management policy for
the nanoESP32-C6-N16 vehicle adapter before treating it as an unattended
installation. The policy must distinguish at least these states:

- **Driving/active:** CAN, GPS, WiFi, AWS and local diagnostics operate normally.
- **Charging:** the vehicle's separate 12-V board-network battery is expected to
  be replenished and more energy is available, so live telemetry and connectivity
  may remain active at a higher duty cycle. Charging must be detected from
  authoritative, debounced signals rather than assumed merely from the presence
  of 12 V.
- **True parked standby:** protect the separate 12-V board-network battery from
  long-term discharge by putting the ESP32-C6 and controllable peripherals into
  an appropriately low-power state. GPS, WiFi and any external mobile hotspot or
  modem must not remain fully active without a measured reason.

Define wake sources and failure behaviour for vehicle/CAN activity, charging,
timer-based reporting, local service access and manual recovery. Measure the
complete adapter current in every state, including both CAN transceivers, the
L76K main and backup rails, the 12-V-to-5-V converter's quiescent current and any
external connectivity hardware. Establish battery-protection thresholds,
transition debounce, maximum wake duration and a fail-safe mode for missing or
contradictory CAN signals. Verify that sleep/wake transitions preserve
configuration, do not create CAN traffic, do not lose completed telemetry and do
not cause reconnect storms.

## Vehicle adapter position and cable layout

Select and document the installation position as a combined RF, electrical,
thermal and serviceability decision. Compare at least the short-cable position
near the vehicle connector with a remotely placed adapter using an optional
longer OBD-II connection cable.

The evaluation must cover GPS sky visibility, WiFi reach to the driver's mobile
hotspot or home network, reception for any external mobile-network device,
shielding by metal/bodywork, antenna separation, temperature, moisture,
accessibility, secure mounting and strain relief. For a longer OBD-II cable,
retain separately twisted CAN-H/CAN-L pairs for both buses, control stub length,
verify conductor assignment and voltage drop under peak load, and avoid routing
along interference sources. Record the selected cable length and adapter position
as part of pilot-installation evidence rather than assuming one position suits
all vehicles.

## Vehicle adapter power supply

Turn the existing pilot wiring guidance into a qualified unattended power-input
design for the separate 12-V board-network battery. Select and validate an
automotive-suitable 12-V-to-5-V stage with input fuse, reverse-polarity and
transient protection, adequate peak-current margin, low quiescent current and
defined undervoltage behaviour. Measure the actual vehicle voltage range during
standby, wake, driving and charging instead of relying on nominal 12 V.

Define connector pinning, wire gauge, fuse placement, grounding, isolation or
common-ground requirements and the interaction with USB servicing. Prevent
backfeed between external 5 V and USB VBUS using a reviewed disconnect or
power-mux arrangement. Qualification must include cold start, brownout, repeated
power cycling, charging-voltage operation, standby drain and recovery after an
undervoltage cutoff. Keep this hardware work aligned with the operating-power-mode
policy above; an efficient converter alone does not provide battery protection.

## Optional local power display

Evaluate a small local display on the ESP32-C6 pilot module as a non-essential
enthusiast extension. The initial use case is a directly visible signed power
display for traction consumption, regeneration and charging power, using the
already decoded Pioneer Standard-CAN battery/vehicle power values without another
CAN interface or cloud dependency.

Prefer a small SPI TFT for a responsive display at a bounded refresh rate. An
ePaper display may be reconsidered for slow-changing parked or charging summaries,
but is not the preferred technology for rapidly changing driving power because of
its update latency, flashing and ghosting. Before implementation, verify free and
boot-safe GPIOs for each board profile, SPI/GPS/Dual-CAN pin coexistence, available
3.3 V regulator margin, display-backlight load, decoupling, cable length, EMC,
strain relief and enclosure placement. Keep display updates cooperative so CAN,
GPS, WiFi, AWS and local services do not regress. Treat the nanoESP32-C6-N16 as
the likely evaluation target; do not assume the pin-constrained XIAO can support
the same option.

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
