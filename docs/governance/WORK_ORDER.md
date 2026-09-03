# Work Order

**Project:** Microlino Open Telemetry (MOT)

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Last reviewed:** 2026-09-01

## High priority

### Add an optional daily journey and charging summary

**Active work package:** [DAY-SUM-001 — Daily Journey and Charging Summary](../project/sprints/DAY-SUM-001.md)

**Objective:** Send one optional email per user, vehicle and Zurich calendar day
with totals from completed journey and charging events, while waiting for active
sessions until a bounded 08:05 deadline.

**Current status:** Backend deployment is complete. The additive default-off
preference, event aggregation, `Europe/Zurich` Scheduler, retry/idempotency
boundary and portal control are implemented. Reviewed Change Set
`day-sum-001-20260903` reached `UPDATE_COMPLETE` without replacing a table or
Lambda function; scheduler and JWT-protection read-backs pass. Hosted portal
upload and controlled live email/deferral acceptance remain open.

### Validate the optional charging-summary email

**Active backend/portal sprint:** [CHG-SUM-001 — Email Charging Summary](../project/sprints/CHG-SUM-001.md)

**Objective:** Send an optional email-only charging summary based exclusively on
standard notification telemetry, with the same 45-second start qualification as
charging-stop detection and completion on unplug or after ten stopped minutes.

**Current status:** Backend deployed and preference migration complete. All 9
existing profiles with both email notifications and journey summaries enabled
now also have the charging summary enabled; no other profile was modified. The
repository portal includes the checkbox and translated wording. Hosted portal
release and acceptance of the first naturally completed charging session remain
open. The implementation does not query the debug table.

### Capture bounded raw telemetry for the xruser charging diagnosis

**Active backend sprint:** [DBG-001 — Bounded Vehicle Telemetry Debug Capture](../project/sprints/DBG-001.md)

**Objective:** Preserve short-lived five-second Display, charging and BMS scalar
telemetry for the explicitly approved `xrpioneer2` identity so Microlino can
diagnose the observed 75%-to-45% SOC reset with pack and cell context.

**Current status:** Deployed and active. The fail-closed path
uses a separate encrypted on-demand table, exact vehicle allowlist, absolute
automatic deadline, seven-day TTL, dedicated write alarm and CSV exporter. GPS,
structured payloads and unrelated vehicles are excluded. Local targeted tests
pass. Replacement-free Change Set `dbg-001-xrpioneer2-20260901` deployed to
`mot-aws-3-1`; the first live query returned 221 `xrpioneer2` rows across the
expected electrical and vehicle signals, while `pioneer` returned zero and no
debug-ingest error appeared. A parameter-only, replacement-free update extended
capture through 2026-09-10 23:59:59 CEST while retaining seven-day per-row TTL;
expiry-stop evidence and the later diagnostic export remain open.

### Deliver a controlled portal Web Flasher

**Active portal/firmware sprint:** [WEBFLASH-001 — Controlled Portal Web Flasher](../project/sprints/WEBFLASH-001.md)

**Objective:** Let an explicitly approved portal user update a locally connected
nanoESP32-C6-N16 or XIAO ESP32-C6 from Chrome or Edge before onboarding, without
choosing a file and without erasing NVS, LittleFS or AWS credentials.

**Current status:** Started on 2026-09-01. The accepted security contract uses a
server-side expiring grant, private immutable application artifact, SHA-256 and
target-specific ESP32-C6/flash preflight, fixed `0x10000` application-only write
and bounded audit. Factory images, arbitrary binaries/offsets and erase controls
remain excluded. The AWS backend is deployed with private versioned
S3, TTL grants, admin grant/revoke, authenticated access/download/result routes and
five-minute direct-download authorization. The bounded catalog now serves XIAO
to Gino and N16 to `info@muehlberg.ch` in parallel with exact target grants.
Portal integration is repository-
complete: admin grant/revoke, authorization-gated German/English/French UI,
vendored Web Serial flasher, exact hardware and SHA-256 preflight, fixed write
range, progress and result audit. The physical B025 N16 update passed. Hosted
portal upload and physical XIAO acceptance remain open. The REV15 release-integrity
follow-up corrected stale embedded REV14 version strings in both catalog targets,
activated exact new object hashes without replacement and added a packaging gate
against manifest/binary version mismatch. `xruser` now has a bounded audited
grant for the corrected XIAO image associated with `xrpioneer2`.

### Reject OTA images built for incompatible adapter hardware

**Completed firmware sprint:** [OTA-HW-001 — Hardware-aware local OTA guard](../project/sprints/OTA-HW-001.md)

**Objective:** Validate the standard Espressif application-image chip ID and
declared flash size before the first OTA flash write, primarily preventing N16
and XIAO images from being interchanged.

**Current status:** Complete. REV13 implements the corrected guard before
`Update.begin()` in the shared C6/WROOM OTA path and LilyGO's local route. A
matching N16 image installed successfully; the XIAO image was rejected for 4 MB
versus 16 MB and the LilyGO image for ESP32 versus ESP32-C6, both before writing.
N16, XIAO, LilyGO AWS and WROOM base builds pass; XIAO remains below its gate at
83.50%. The current WROOM AWS aggregate exceeds its already constrained slot by
3,344 bytes and remains a separate release blocker. Boards with identical chip
and flash geometry remain outside this guard and would need a future signed MOT
manifest.

### Localize the pilot portal and user handout

**Active portal work package:** I18N-001

**Objective:** Keep German as the project language and dashboard default while
making the pilot-facing portal and one-page handout available in English and
French. The local firmware wizard remains English-only.

**Current status:** Repository implementation and visual desktop/smartphone
acceptance are complete. The portal persists a `de`/`en`/`fr` selection, updates
locale-sensitive dates and History charts, and retains German as fallback. The
reproducible one-page generator produces German, English and French A4 PDFs.
Hosted upload and native-speaker review of French wording remain open.
The public root landing page now also carries the persisted three-language
selector and links to the dedicated interactive `/onboarding/` page. Static contracts,
JavaScript checks and local desktop/390 px browser acceptance pass; hosted upload
and maintainer acceptance remain open.

### Complete guided C6 local onboarding

**Completed firmware sprint:** [ONB-UX-001 — Guided C6 Local Onboarding](../project/sprints/ONB-UX-001.md)

**Objective:** Let a standard user complete protected local setup, connectivity,
CAN selection, optional-service configuration and validation through one coherent
flow with one consolidated apply-and-restart boundary.

**Current status:** Repository implementation and physical acceptance completed
on 2026-08-25. B025 passed repeated factory-first-run walkthroughs; B021, B023 and
B024 received configuration-preserving firmware updates. The onboarding guide and
one-page handout reflect the accepted flow. Optional History, email and SMS
activation stays in the future administrator-tool work package.
The repository also contains the first pilot-feedback refinement: clickable local
URLs, visible action progress, a compact GPS control and fixed Display-CAN
presentation for the hard-wired CAN2 input. Physical acceptance remains pending.
The 2026-09-01 N16 refinement also removes the intermediate WiFi, CAN and service
restarts: the user reviews all non-secret settings, applies them once, then sees the
real WLAN/IP state and performs runtime validation after the single restart.

### Make range basis and SOC reserve configurable

**Active portal sprint:** [RNG-SET-001 — Personal Range Settings](../project/sprints/RNG-SET-001.md)

**Objective:** Use explicit per-user/per-vehicle full-range and reserve settings
for both fixed and learned range forecasts while retaining 140 km / 0% defaults.

**Current status:** Repository API, persistence, calculation and compact settings
fields are implemented. The isolated Preference Lambda update is deployed and
healthy; the matching dashboard is hosted and the maintainer confirmed that both
values persist and affect the displayed result. Cross-vehicle validation remains
open. A dedicated settings page with an explicit return-to-dashboard button is a
documented follow-up.

### Mark stale charging and power values

**Completed portal sprint:** [PWR-FRESH-001 — Charging and Power Freshness](../project/sprints/PWR-FRESH-001.md)

**Objective:** Keep retained charging and power values visible while making it
immediately clear in the overview and net-power History when their source topics
have stopped updating.

**Current status:** The repository implementation uses mode-specific topic
timestamps, dims stale contents after the existing 120-second boundary and shows
`Nicht aktuell · letzter Messpunkt hh:mm`. All 185 portal/tool tests pass and
hosted desktop and smartphone acceptance of overview and net-power History
completed on 2026-08-24. The sprint is closed.

### Restore Lambda capacity and isolate interactive APIs

**Active work package:** [OPS-001 — Lambda Capacity and Portal Resilience](../project/sprints/OPS-001.md)

**Objective:** Remove the exceptional regional ten-concurrency bottleneck, keep
interactive Vehicle/History/Notification APIs responsive during telemetry bursts
and make the portal retain independently successful data when one read fails.

**Current status:** A live incident on 2026-08-24 reached the full 10/10 regional
Lambda concurrency pool for at least seven consecutive minutes and produced
385–620 throttles per minute. State ingest and notification processing were the
principal consumers; DynamoDB was not throttled. The repository portal now
retries only transient GET failures and uses independent preference/SMS results,
so one failed read is no longer presented as several empty or unconfirmed
settings. AWS initially rejected a request for 100 because the Service Quotas
workflow compared the effective value 10 with a nominal default of 1,000. Live
read-back on 2026-08-25 now reports the effective regional quota restored to
1,000; the queue/isolation design and hosted acceptance remain open. Two
account-level CloudWatch alarms use the confirmed operations topic, notify only
on `ALARM`, and the concurrency warning is aligned to 800. The repository
Notification Rule now filters irrelevant topics before
Lambda invocation. Its first packaged Change Set was deliberately not executed
and removed because packaging drift also proposed unrelated Lambda, integration
and journey-finalizer modifications.
After explicit approval, AWS accepted the 1,001-concurrency request and opened
support case `178759663200182`; the restored effective value of 1,000 closes the
capacity gate. A second Change Set derived from the live template modified only the
Notification IoT Rule. Its first SQL form rolled back safely on an AWS IoT syntax
error; the corrected array-literal form then reached `UPDATE_COMPLETE` with no
replacement. Live SQL read-back passes. The first full post-filter minute reduced
notification invocations from 187–206 to 4, notification throttles from 115–139
to 5 and account throttles from 225–265 to 18. State-ingest isolation and hosted
portal acceptance remain open.

### Activate controlled SMS notifications

**Active work package:** [SMS-001 — Controlled SMS Notification Pilot](../project/sprints/SMS-001.md)

**Objective:** Deliver the existing SOC-target and qualified charging-stop events
through a bounded SMS pilot only after AWS service activation, administrator
approval, Swiss destination and sender allowlisting, an enforced spend limit and
an operational alarm are deployed as one fail-closed package.

**Current status:** Analysis is complete and AWS has approved End User Messaging
SMS production access in `eu-north-1` with a maximum monthly text quota of USD
50. The project deliberately enforces a lower USD 10 monthly override and a USD
6 CloudWatch warning. A deletion-protected account-default Protect configuration
blocks 244 countries and allows only CH; it is also associated with the fixed
`mot-dev-sms` configuration set. The deletion-protected transactional `MOT`/CH
sender has zero monthly lease cost but is reported by AWS as unregistered. Alarm
subscriptions for `info@muehlberg.ch` and
`support@microlino-open-telemetry.ch` are independently confirmed. A controlled
CloudWatch test exercised the operations topic, reached `ALARM` and returned to
`OK` without sending an SMS or incurring SMS spend; the maintainer confirmed
receipt at both destinations. SMS-001.C is now deployed additively: encrypted
approval and 90-day audit tables plus an exact-principal, table-only admin role.
The non-echoing plan/apply CLI stores only the destination fingerprint and writes
approval plus audit atomically with optimistic version checks. The reviewed
Change Set added exactly those three resources without modification, replacement
or deletion; an unauthorized probe failed before write.
Current AWS list price remains USD 0.05124 per Swiss message part;
estimated total implementation and acceptance effort remains 6.5–12 engineering
days. SMS-001.E backend deployment is complete: the portal source can
self-register and verify a Swiss number through a least-privilege API, reuse its
fingerprint across users or vehicles and expose activation only for the exact
approved association. Both reviewed, replacement-free Change Sets reached
`UPDATE_COMPLETE`; unauthenticated probes fail. The hosted portal subsequently
delivered and accepted the first
verification code after two least-privilege corrections: unnecessary creation
tags were removed and the implicit send permission was restricted to the exact
`MOT/CH` sender. AWS now reports the controlled destination `VERIFIED`, and its
exact Pioneer association has a 30-day version-1 administrator approval with an
atomic audit record. SMS-001.D is now deployed and activated after a disabled
staging pass: it gates SOC-target and qualified charging-stop SMS on exact
approval, verification, fixed `MOT/CH` sender, USD 10 live limit, `OK` spend alarm,
single-part format, idempotency and ten-per-destination/day rate state. Journey
summaries remain email-only. The old SNS wildcard permission was removed, the
Pioneer opt-in is restored and the new rate table is empty. Physical delivery and
cost reconciliation remain pending.

The first live reconciliation measured USD 0.61488 and correctly tripped the
initial USD 0.60 alarm while email delivery continued. AWS later approved
production access with a USD 50 maximum. On 2026-08-29 provider override and
Lambda expectation were aligned to USD 10 before the alarm was raised to USD 6.
All three controls now pass live read-back, the alarm is `OK`, recovery emails
remain disabled and current measured spend is USD 0.92232.

### Detect charging interruption and prepare controlled SMS

**Completed work package:** [NTF-002 — Charging Interruption Notifications](../project/sprints/NTF-002.md)

**Objective:** Detect a persistent transition from charging to still-plugged below
the configured target, deliver one idempotent email after a durable 60-second
validation and place any later SMS delivery behind explicit administrator approval
and spend controls.

**Current status:** The independent default-off Ladestopp checkbox and SOC target
are repository-complete. The email backend is deployed with an encrypted durable
60-second SQS validation, DLQ, falling-edge/cancellation state and idempotent
per-user delivery. Live read-back confirms unchanged existing preferences plus the
new default-off fields, and an invalid delayed probe produced zero deliveries.
The static portal was uploaded and the controlled owner enabled a separate 100%
Ladestopp target. A first brief startup-charge/stop sequence produced exactly one received
email at 89% against a 100% target. That evidence triggered a deployed 45-second
continuous-charging qualification and a hard one-delivery-per-user/vehicle/session
idempotency boundary. The final controlled session charged for longer than 45
seconds, stopped at 91% against the independent 100% target, remained plugged and
produced exactly one stored event and one received email after the 60-second
validation; the queue emptied and Lambda logs remained error-free. The email
scope is closed. The AWS account remains unsubscribed from End User Messaging SMS
in `eu-north-1`; SMS activation remains a separate work package.

### Combine binary charging History

**Completed portal sprint:** [HIS-BIN-001 — Combined Binary Charging History](../project/sprints/HIS-BIN-001.md)

**Objective:** Combine charging and plugged state in one compact chart and render
both as discrete steps without diagonal interpolation across missing samples.

**Current status:** The combined chart passed hosted desktop/smartphone
acceptance. The 7-day refresh correction also passed hosted acceptance. A 30-day
follow-up found confirmed Vehicle API timeouts/throttling and an unintended full
History reload every five seconds. The repository fix removes that polling load,
retains the last successful charts on transient failures and increases Vehicle API
runtime capacity. Hosted 24h/7d/30d testing passed across different users on
desktop and smartphone. The live Vehicle API runs at 256 MB/25 seconds, a direct
authorized 30-day request passed with 51 points, and the reviewed CloudFormation
reconciliation reached `UPDATE_COMPLETE`. The sprint is closed.

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
Standard-CAN charge boundary exposed one false split during continuous movement.
The deployed backend now rejects plug/charge assertions while movement is newer
than two minutes unless speed zero was observed. The matching REV8 firmware makes
Standard-CAN V1/V2 exclusively authoritative by suppressing Display-CAN `0x603`
and `0x604`; N16 and XIAO builds pass, while OTA installation and a repeat road
observation remain open. An independent
30-minute inactivity fallback for a missing terminal signal is also deployed;
its physical no-coverage garage observation remains open. Multi-journey energy
calibration and cost evidence remain open. REV11 now implements the RAM-only
drawn/regenerated Wh counter for nanoESP32-C6-N16 with 60-second and journey-end
checkpoints. The final N16 build uses 58,600 bytes RAM and 1,377,732 bytes flash;
XIAO keeps the counter disabled and passes its 85% OTA-slot gate at 83.40%.
Backend freshness fallback, physical runtime-heap observation and a controlled
road comparison remain the rollout gates. The first REV13 comparison showed that
the final firmware checkpoint can follow the charge-boundary topic. The
repository backend now seals immediately, waits at most 15 seconds for a complete
fresh counter and otherwise retains the telemetry-estimate fallback for older
firmware. The isolated Notification Lambda update is deployed and healthy;
positive road validation remains open. A following 10 km journey delivered its
final firmware checkpoint about 5 minutes 40 seconds after the telemetry summary.
The repository now gives normal `speed_zero` completions a further bounded
ten-minute firmware wait after stop confirmation, without delaying timeout or
exclusion decisions. The isolated Notification Lambda update is deployed and
healthy. A later 47 km journey exposed a separate payload-contract defect:
REV13 published the counter ID as bare text although the Notification Lambda
required JSON. REV14 publishes a quoted JSON string; the deployed backend also
accepts the narrowly validated legacy ID only on that topic, so already deployed
REV13 devices are immediately compatible. Both C6 builds, 74 notification tests,
six firmware counter contract tests and a live REV13-format probe pass. Physical
journey validation remains open.

A subsequent REV14 drive isolated the remaining fallback cause as DynamoDB
session contention during a burst after weak/mobile-to-Home connectivity. The
Journey state previously shared one optimistic record with charging and summary
updates, and some counter fields exhausted four lockstep retries. The repository
now migrates Journey state into a namespaced per-vehicle item within the same
table and uses twelve bounded jittered retries among Journey topics. Existing
active/cache state remains compatible and the scheduler can migrate it without a
new signal. All 81 notification tests and syntax validation pass; AWS deployment
is complete and a synthetic isolated-session create/read/cleanup smoke test
passes. Physical recovery validation remains open.

A later short REV14 trip followed by immediate charging showed that its complete
firmware checkpoint arrived about 6 minutes 45 seconds after the email during
the mobile-to-Home transition. The former 15-second charge-boundary grace is now
ten minutes: sealing remains immediate, complete fresh counters finish early,
and older or delayed firmware retains the telemetry fallback after the bound.
All 82 notification tests pass. The isolated Notification Lambda update is
deployed and reports `Active`/`Successful`; road validation remains open.

The 49 km History also showed a 13-minute 52-second intermediate stop that was
incorrectly reopened because the optional firmware wait extended beyond the
ten-minute journey boundary. The repository now finalizes the old journey before
accepting resumed movement after ten stop minutes, then applies the same movement
sample to a new journey. The extra counter wait remains available only while the
vehicle stays stopped. All 80 notification tests pass; the isolated Notification
Lambda update is deployed and healthy. Road validation remains open.

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

### Execute the bounded LilyGO mobile dual-CAN pilot

**Active work package:** [LG-CAN2-001 — LilyGO Mobile Dual-CAN Pilot](../project/sprints/LG-CAN2-001.md)

Retain the three available LTE-capable LilyGO adapters for flexible field pilots
by adding an Adafruit MCP2515 receive-only CAN2 input. Repository implementation
and the `T-A7670X-AWS` build pass at 77.4% of each enlarged OTA slot. Controlled
USB partition migration and bench discovery pass; simultaneous dual-bus vehicle
reception remains a hardware gate and WiFi/LTE switching passed on a road test.
The measured OTA margin supports a bounded, default-off 128 KiB SOC/Speed cache
follow-up under the separate `pioneer-lilygo` identity. This sustain-only package
does not change the C6 strategic target.

**Active bounded extension:** [LG-S3-001 — T-SIM7670G-S3 Firmware Pilot](../project/sprints/LG-S3-001.md)

Prepare the newly available N16R2 board using the shared full LilyGO feature set,
an isolated `pioneer-sim7670` default identity and board-specific SIM7670/GNSS/CAN
integration. Source/build qualification is complete; perform the listed bench and
vehicle gates before treating LTE TLS, GNSS or dual-CAN as physically accepted.

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
