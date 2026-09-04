# CACHE-001 — Optional Offline SOC/Speed Cache

> **Status:** Closed — acceptance passed; dedicated B025 AWS resources retired
>
> **Date:** 2026-08-20

## Objective

Add an explicitly optional, default-off store-and-forward cache for SOC and
vehicle Speed during temporary Internet loss. Preserve current CAN processing,
live AWS telemetry, History authorization and per-user/per-vehicle isolation.

The first physical pilot uses the dedicated B025 nanoESP32-C6-N16 test module
without GPS. No XIAO hardware will be procured for this sprint; its conservative
build and storage boundaries remain compatibility checks only.
The productive adapter and its vehicle identity are not test targets for the
development or initial cloud validation.

## Product and privacy boundary

- Cache only SOC and vehicle Speed.
- Do not store or backfill GPS coordinates, routes, odometer or other telemetry.
- GPS is recommended, but not required, for adapters using the offline cache.
  Existing C6 firmware automatically sets system UTC from a valid GNSS date/time;
  the cache consumes that same system clock without location data. B025 has no GPS
  and proves that prior NTP time is sufficient while power remains uninterrupted.
- Require confirmed UTC from NTP before recording samples. A cold offline boot
  without trustworthy time records nothing.
- Expose one authenticated local-WebUI option, default off.
- Disabling the option and factory reset purge pending cached telemetry.
- Configuration backup may contain the option but never cached samples.

## Sampling and storage contract

- SOC uses a bounded multi-minute cadence while driving or charging.
- Speed uses the existing active one-minute History cadence and records a
  terminal zero when movement ends; continuous standstill zeroes are suppressed.
- Store compact, versioned and checksummed fixed-size records in a bounded
  append-only LittleFS journal.
- Reserve at most 128 KiB on XIAO and 256 KiB on nanoESP32-C6-N16.
- When the hard byte/sample ceiling is full, stop accepting new records and count
  drops instead of continuously erasing and rotating flash.
- Delete a local batch only after an authenticated acknowledgement. Replay and
  acknowledgement retries must be idempotent.
- Diagnostics expose enabled state, pending count, oldest age, dropped count and
  last replay result without providing a bulk telemetry dump.

## Isolated AWS test lane

The first backend test must not run through the operational `mot/#` ingestion
path. Create a minimal separate CloudFormation stack in the existing AWS account
and region with:

- a dedicated MQTT root such as `mot-test/#`;
- one test Thing, certificate and least-privilege policy for the separate test
  module;
- a History-backfill Lambda and a separate DynamoDB table;
- a three-day DynamoDB TTL and seven-day log retention;
- separate metrics, errors and consumed-write visibility;
- no Cognito, portal, live State table or WebSocket copy;
- a small tagged cost boundary and AWS Budget warning.

The production rule must not receive test messages. The test handler accepts only
the configured test root and writes only its test History table. The stack must
be independently removable without touching operational resources.

## Backfill protocol

Use a separate versioned history-only envelope carrying original timestamps,
signal identifiers, bounded samples and an idempotent batch identifier. Apply
strict payload, sample-count, age and timestamp-range validation.

Backfilled samples must never update current State or generate live WebSocket
events. The backend must reject unknown vehicles/signals, invalid timestamps,
oversized batches and unauthorized topic/vehicle combinations. Its acknowledgement
must not be re-ingested as telemetry or create a publish loop.

## Delivery slices

| Slice | Outcome | Status |
|---|---|---|
| CACHE-001.A | Freeze sampling, journal, replay and MQTT contracts | Complete |
| CACHE-001.B | Build isolated `mot-test` AWS lane with cost and TTL guards | Deployed; test-device policy update and Budget destination pending |
| CACHE-001.C | Implement shared C6 cache core and authenticated WebUI control | Repository complete |
| CACHE-001.D | Retain XIAO build and storage compatibility gates | Current build passes the strict gate at 82.40%; no physical XIAO test planned |
| CACHE-001.E | Validate offline/reconnect/backfill on no-GPS B025 N16 | Physical SOC and moving-Speed outage/reconnect pass |
| CACHE-001.F | Validate N16 filesystem, power and replay recovery | Hard-power and in-flight ACK-loss recovery pass; conservative flash-life gate passes |
| CACHE-001.G | Prepare disabled production integration and one-device rollout plan | B025 production promotion and real replay pass |

## Acceptance gates

- feature remains off by default on both C6 targets;
- no valid UTC means no cached record;
- loss of WiFi association alone is not the trigger: AWS IoT delivery state is
  authoritative;
- reconnect publishes fresh live state before bounded background replay;
- duplicate, reordered and partially acknowledged batches create no duplicate
  logical History samples and lose no unacknowledged local samples;
- power loss during append, replay and acknowledgement recovery leaves the
  journal structurally valid;
- malformed or stale backfill cannot reach live State or WebSocket paths;
- the XIAO canonical application remains below the strict 85% OTA-slot gate and
  retains adequate LittleFS space and runtime heap;
- CAN processing, WiFi recovery, local WebUI, AWS live publication and existing
  History behavior do not regress;
- measured write amplification supports a conservative flash-life assessment;
- test AWS resources are tagged, TTL/log retention is active and actual test cost
  is measurable;
- no deployment to the operational stack occurs until isolated test evidence is
  reviewed.

## Cost and flash-life gate

The intensive isolated physical test period remained inside the AWS free tiers:
Cost Explorer reported USD 0 for IoT Core, Lambda, DynamoDB and CloudWatch. The
isolated lane accumulated 19 Lambda invocations, 8.31 seconds total Lambda
duration, 108 consumed DynamoDB write units, 25 read units and zero Lambda errors.
At the measured pilot cadence this is materially below one cent even when valued
outside the free tiers; normal B025 use is lower than the fault-injection run.

Each local journal record occupies 24 bytes. The N16 limit of 256 KiB therefore
holds at most 10,922 records. At the maximum configured combined cadence of 12 SOC
and 60 Speed records per continuous offline hour, that is about 152 offline hours.
Two fully offline driving hours per day create at most 144 records or 3.4 KiB of
logical journal data per day and fill the cap only after about 76 days.

For a deliberately conservative wear estimate, assume 16 KiB physical flash work
per appended record and only 10,000 erase cycles. Two offline driving hours every
day then correspond to about 2.25 MiB physical writes per day, or 0.38 whole-
LittleFS-partition writes per day on the 6.16 MB N16 partition. The resulting
theoretical endurance is over 70 years before allowing for wear levelling. This
is an engineering bound, not a flash-vendor guarantee; real writes occur only
while AWS delivery is unavailable. The bounded no-rotation full-cache behavior
and the default-off product switch remain the primary lifetime safeguards.

## Required test scenarios

1. Online operation with cache disabled.
2. Online operation with cache enabled and no queued samples.
3. Valid NTP time followed by hotspot Internet loss during movement.
4. Cold boot without Internet/NTP, confirming that no samples are invented.
5. Reconnect with current live publication followed by oldest-first replay.
6. Broker/Lambda acknowledgement loss and duplicate replay.
7. Power interruption during journal append and during replay.
8. Full cache behavior without erase churn.
9. Invalid, future, expired and oversized backend envelopes.
10. Proof that the operational adapter, State table and WebSocket stream remain
    unaffected throughout the isolated test.

## Production rollout boundary

After isolated acceptance, integrate the backend additively behind a disabled
feature flag. Review a CloudFormation Change Set for replacements, run existing
live/authorization regressions, then allowlist only one explicitly approved pilot
vehicle. Disable the flag for rollback; do not require stored History migration.

## Deferred work

- GPS/location caching and route reconstruction;
- hardening of the physically validated GPS-derived UTC cache path. A later work
  package must reject stale/implausible receiver time and define deterministic
  GPS/NTP precedence while continuing to use time only, never persist or
  backfill coordinates, and retain NTP operation for hardware without GPS;
- encryption of cached telemetry against physical flash access;
- general fleet enablement and per-user cloud configuration;
- a complete cloned Cognito/API/portal test environment;
- offline caching of power, battery-cell or future Standard-CAN signals.

## Validation evidence

On 2026-08-20:

- stack `mot-cachetest` reached `CREATE_COMPLETE` with only new isolated
  resources; its exact topic root is `mot-test`, table TTL is enabled at three
  days and Lambda memory/timeout are 128 MiB/10 seconds;
- one synthetic two-sample batch stored SOC and Speed in
  `mot-cachetest-vehicle-history`; identical replay kept the row count at two and
  logged `stored=0`, `duplicates=2`;
- the operational `mot-dev-vehicle-state` table contained zero records for
  `cache-xiao-01` after that synthetic test; the physical identity is being
  renamed to `cache-b025-n16-01` before live testing;
- eight local backend tests cover topic/vehicle isolation, duplicate replay,
  terminal Speed zero, signal/value/time validation and acknowledgement behavior;
- eight CACHE-001 firmware contract tests and the complete 155-test repository
  suite pass;
- canonical builds pass: XIAO remains within its compatibility gate; the
  current N16 image uses 1,346,880 bytes and physically reports 16 MiB flash;
- `git diff --check` passes;
- a first stack creation attempt was fully rolled back because the account's low
  Lambda quota cannot reserve concurrency while preserving ten unreserved
  executions. The deployed template omits reserved concurrency and retains exact
  topic, payload, batch, TTL and account-quota limits instead;
- a dedicated certificate and least-privilege test policy are installed on B025;
  physical startup confirms the N16 profile, 256 KiB enabled cache, valid test
  credentials, AP `MOT-4085D9` and no operational AWS path. No operational stack
  was changed;
- the first iPhone-hotspot connection exposed that AWS IoT authorizes retained
  Last Will and retained Birth metadata through the separate `iot:RetainPublish`
  action. The test policy now grants that action only for `status/online` and
  `system/*`; B025 then connected in 1.75 seconds, published all 11 Birth values
  and remained connected. Its on-device certificate SHA-256 matched the attached
  AWS certificate exactly;
- the physical CAN/charging-bench run continued both CAN inputs with zero
  controller errors after the iPhone association timed out. B025 cached three SOC
  samples (89, 88 and 87 percent) at their confirmed UTC sample times. After the
  hotspot returned, AWS live publication stabilized, one acknowledged backfill
  batch stored exactly those three `offline-backfill-v1` records in the isolated
  table, and the local journal reached `pending=0`, `replayed=3`, with zero drops,
  corruption, duplicates or rejections;
- on 2026-08-21 a physical return drive with the hotspot disabled added 13 new
  records: two SOC samples and eleven Speed transition/minute samples. The Speed
  series included 1.5, 31.5, 55, 41.5, 72.5, 25 and 27 km/h followed by three
  terminal-zero transitions. Reconnection acknowledged two bounded batches,
  preserved every original timestamp and left `pending=0`, `replayed=16`
  cumulatively, with no drop, corruption, duplicate or rejection. The cache only
  writes zero on an active-to-inactive transition. The maintainer confirmed that
  these transitions fall inside the final parking manoeuvre; after the definitive
  stop and start of charging, no further zero was stored. Continuous-standstill
  suppression therefore passes and no speculative debounce is added;
- later on 2026-08-21, seven queued records survived full power removal while
  the hotspot was disabled. After cold restart LittleFS mounted with `pending=7`,
  `corrupt=0`, `dropped=0`; both CAN controllers resumed with zero errors, AWS
  made no connection attempt and the invalid cold-start clock created no new
  sample. Hotspot/NTP recovery then added one correctly timed SOC sample before
  AWS finished connecting. One acknowledged eight-record batch stored two SOC
  and six Speed records, including terminal zero, with original timestamps. The
  journal reached `pending=0`, `replayed=8`; all 85 isolated physical test rows
  have unique logical signal/timestamp keys;
- the controlled in-flight acknowledgement test temporarily set the isolated,
  default-true `TestAckReceiveEnabled` switch to `false`. AWS stored the first
  eight records while B025 retained all nine at `waitingForAck=true`; power was
  removed before acknowledgement. After restoring the switch to `true`, reboot
  and fresh CAN publication replayed the identical batch. DynamoDB added no
  rows, the ACK reported exactly eight duplicates, and the ninth record was
  accepted separately. A later brief hotspot loss added two normal records;
  final state was `pending=0`, `replayed=11`, `duplicates=8`, with zero drop,
  corruption or rejection. The stack is `UPDATE_COMPLETE` and the test switch
  remains at its safe `true` default.
- Cost Explorer and isolated CloudWatch/DynamoDB metrics close the pilot cost
  gate at USD 0 billed for the test period, with 19 Lambda invocations, 8.31
  seconds total Lambda duration, 108 writes, 25 reads and zero Lambda errors;
- on 2026-08-21 the operational stack first received only the fail-closed live-
  ingest guard while Backfill remained disabled. Its Change Set modified the
  State Lambda and IoT rule in place with no replacement. A direct production
  guard test returned `history_backfill_isolated` and created zero State rows;
- a second reviewed Change Set added exactly the dedicated role, 128 MiB/10 s
  Lambda, seven-day log group, invoke permission and exact
  `mot/+/history/backfill/v1` rule. The stack reached `UPDATE_COMPLETE`, Backfill
  is enabled only for `cache-b025-n16-01`, and a two-sample production smoke batch
  stored exactly one SOC and one Speed History row while State remained empty.
- B025 was then provisioned with the separate production Thing
  `mot-esp32c6-4085d9`, individual certificate and exact
  `mot/cache-b025-n16-01` namespace. Its 16 MiB image and LittleFS upload passed,
  the retained NVS configuration rejoined the iPhone hotspot, AWS connected in
  1.76 seconds and all 11 Birth fields reached operational State;
- the first real production outage occurred while B025 was connected to both CAN
  buses. CAN1/CAN2 continued to 64,187/27,195 frames with zero controller errors,
  one SOC sample at 87 percent was cached and replayed after mobile connectivity
  returned. The ACK reported `stored=1`, `duplicates=0`; the local journal ended
  at `pending=0`, `replayed=1` with no drop, corruption or rejection. State
  independently held the fresh live SOC 87, Speed 0 and online=true. The two
  explicitly marked synthetic smoke rows were then conditionally deleted, leaving
  only the real physical B025 replay in its History partition.
- the confirmed, email-verified existing `news@muehlberg.ch` portal identity was
  assigned B025 through the controlled admin-onboarding path. The resulting
  `UserVehicleAccess` record is `ACTIVE/OWNER`, carries the non-personal source
  `cache-001-b025-prod`, and was created only after confirming existing State and
  the absence of any assignment or ownership conflict. No firmware, certificate
  or vehicle namespace change was needed.
- the existing productive N16 `MOT-80A2DA`/Thing `mot-esp32c6-daa280` was promoted
  next under its unchanged `pioneer` identity. Its on-device partition table was
  read back and matched `partitions_16mb.csv` byte-for-byte, so only the
  bootloader/identical partition table/application were flashed; LittleFS, AWS
  credentials and NVS settings were not rewritten. It rejoined HOME WiFi and AWS
  with the original identity, reported `C6-001-REV7-AWS`, retained configured
  local administration and mounted an empty 256 KiB cache. The cache was then
  explicitly enabled. IoT policy version 2 grants only the additional Pioneer
  Backfill-ACK receive path, and the operational Backfill allowlist now contains
  exactly B025 plus `pioneer`; the stack is `UPDATE_COMPLETE`.
- the subsequent physical Pioneer charging outage passed. Six locally timestamped
  SOC samples (77, 79, 80, 82, 83 and 86 percent) from 19:10 through 19:38 were
  received together at 19:38:46 and acknowledged with `stored=6`,
  `duplicates=0`. A later one-sample retry of the final bucket was safely reported
  as one duplicate and created no second History row. The device ended online
  with an empty mounted cache, no drop, corruption or rejection; both CAN paths
  continued with zero controller errors. This closes the Pioneer physical gate
  before upgrading provisioned inventory adapters.
- the GPS-equipped Pioneer then passed the offline cold-start clock path. With
  HOME and hotspot unavailable, a real reboot reset AWS counters to zero; GNSS
  became valid and logged `MotGps: system UTC set from GNSS epoch=1787335905`
  before any network connection. Offline sampling subsequently replayed nine
  records after HOME returned. AWS accepted an eight-sample batch with six new
  rows/two idempotent bucket duplicates and a final one-sample batch with one new
  row. Stored data included SOC plus active/terminal Speed with original times;
  the cache ended `pending=0`, `replayed=9`, `duplicates=2` without drop,
  corruption or rejection. No location entered the cache or Backfill payload.
- a targeted DynamoDB review then confirmed four offline SOC samples at 20:10,
  20:15, 20:20 and 20:25 CEST. All carry `source=offline-backfill-v1` and were
  received together only at 20:25:24 after HOME WiFi returned. This closes the
  independent GPS-time, local-journal and delayed-replay acceptance gate.
- on 2026-09-03 replacement-free Change Set
  `cache-xrpioneer2-backfill-20260903` added `xrpioneer2` to the existing
  operational Backfill allowlist for diagnosis of recurring connectivity/power
  gaps. No retention, table, debug or other vehicle parameter changed. Stack and
  Lambda environment read-back pass. Subsequent maintainer acceptance confirmed
  that the deployed cache and replay behaviour works as intended; no additional
  CACHE-001 acceptance gate remains.

## Closure

CACHE-001 closed on 2026-08-21. The optional, default-off N16 cache, bounded
production Backfill path, authorization/isolation controls, flash-life and cost
gates, power-loss/idempotency recovery, physical moving-Speed/SOC tests and the
GPS-derived UTC cold-start path all pass. Further GPS clock plausibility work is
tracked separately and does not reopen this sprint.

After closure, provisioned inventory modules B021 through B024 received the same
verified N16 application image by an app-only write at `0x10000`. Each device
reported 16 MB flash and a byte-identical partition table before writing. Runtime
checks confirmed the expected individual identity, retained AWS credentials,
mounted LittleFS and an empty 256 KiB cache that remains disabled by default:
`MOT-EC18DB`/`ml-pilot-021`, `MOT-EC34DA`/`ml-pilot-022`,
`MOT-A0AED9`/`ml-pilot-023` and `MOT-4495D9`/`ml-pilot-024`. No NVS,
LittleFS or credential partition was rewritten.

The post-closure XIAO compatibility rebuild also passes: 30 focused C6/cache
contracts succeeded and `xiao-esp32c6` produced a 1,404,096-byte OTA binary
(SHA-256 `102f179591dbefd36d29d9c3d943ed80343e6baf517656d4b059e4788e972e16`).
The strict binary-size gate reports 82.40% of its 1,703,936-byte app slot, leaving
44,249 bytes to the 85% project ceiling. Its current 4 MB layout retains two OTA
slots plus 640 KiB LittleFS; the cache remains capped at 128 KiB. A XIAO already
using this exact layout can take the application by OTA, while an older or unknown
partition layout requires a read-back check and potentially one controlled USB
migration because OTA cannot replace the running partition table safely.

The production preparation for Gino's existing XIAO identity was then completed
without enabling its device-side cache. CloudFormation Change Set
`cache-ginopioneer-20260821` modified only the Backfill Lambda and IoT rule in
place with no replacement; the stack returned to `UPDATE_COMPLETE` with effective
Backfill allowlist `cache-b025-n16-01,pioneer,ginopioneer`. Thing
`mot-esp32c6-a3bd10` retains its exact `ginopioneer` namespace, and policy version
2 adds only Subscribe/Receive on
`mot/ginopioneer/history/backfill/ack/v1`. The local recovery policy copy matches
AWS. No synthetic row was written to Gino's History during this preparation.

## B025 retirement

On 2026-08-26 the maintainer explicitly retired the dedicated B025 test adapter
from AWS after CACHE-001 evidence was complete. The productive certificate was
first detached and made inactive. A reviewed no-replacement Change Set removed
`cache-b025-n16-01` from the operational History and Backfill allowlists. AWS then
deleted 28 State rows, 177 History rows, its `ACTIVE/OWNER` account association
and Notification session; no Preference, Journey event, SMS, Claim, Ownership or
live-connection rows existed.

The productive Thing `mot-esp32c6-4085d9`, its policy and certificate were
deleted. The dedicated `mot-cachetest` stack was deleted with its Test Thing,
policy, Lambda, IoT rule, alarm, log group and empty TTL table; its externally
provided test certificate was subsequently deactivated and deleted. Negative
read-backs found no remaining B025 Thing, certificate, data partition, account
association or active stack. Historical sprint evidence and documentation are
retained as audit material, not active adapter resources.

## B021 operational enablement

On 2026-08-28 the maintainer explicitly enabled B021
`MOT-EC18DB`/`ml-pilot-021` for cloud History and optional offline Backfill. The
reviewed Change Set `enable-ml-pilot-021-history-20260828` added only that vehicle
to both operational allowlists; all four derived Lambda/IoT-rule modifications
were in place with `Replacement=False`, and the stack returned to
`UPDATE_COMPLETE`. IoT policy version 2 grants Subscribe/Receive only on
`mot/ml-pilot-021/history/backfill/ack/v1`; the existing vehicle-scoped Publish
permission already covers its Backfill upload. Final read-back confirmed both
Lambdas active with successful updates and effective allowlists containing
`ml-pilot-021`. The physical adapter cache remains default-off until explicitly
enabled in the authenticated local wizard.
