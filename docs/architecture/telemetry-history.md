# Telemetry History Pilot

> **Status:** Pilot, minute-aggregated Speed and range forecast backend deployed
>
> **Work package:** HIS-001
>
> **Last verified:** 2026-08-04 locally and against the AWS development stack

## Existing flow and isolation boundary

AWS IoT Core sends every `mot/#` publication to `StateIngestFunction`. The
function conditionally updates one `vehicle-state` row per `vehicleId` and topic
suffix, then independently fans a successfully stored update out to authorized
WebSocket connections. `UserVehicleAccess` is checked by REST and WebSocket paths.

The history pilot is a side effect only after the existing state update succeeds.
History write failures are caught and logged, so they do not suppress current
state or live fan-out. The master feature flag defaults to off. When enabled, SOC
and charging are the first phase; the separate motion-signal flag for Speed also
defaults to off. An explicit vehicle allowlist is required and an empty
list writes nothing. The browser-local IndexedDB recorder remains a
legacy-MQTT fallback and is no longer the AWS source.

## Pilot data model

Table: `${ProjectName}-${Environment}-vehicle-history`, on-demand billing.

| Key/attribute | Meaning |
|---|---|
| `vehicleId` (PK) | Existing canonical vehicle identity |
| `sampleKey` (SK) | `<signal>#<UTC bucket epoch seconds>` |
| `sampledAt` | Bucket start in epoch milliseconds |
| `receivedAt` | Timestamp of the first accepted value in the bucket |
| `value` / `valueType` | Normalized source value |
| `expiresAt` | DynamoDB TTL, at most 31 days after bucket start |

No user identifier is duplicated into history. Access is always derived from the
existing ACTIVE user/vehicle assignment.

## Cadence, API resolution and retention

| Signal | MQTT topic | Stored cadence | Portal/API resolution |
|---|---|---:|---|
| SOC | `display/soc` | 5 minutes | 5 min / 30 min / 2 h |
| Interner SOC (optional) | `bms/soc_internal` | 5 minutes | 5 min / 30 min / 2 h |
| Charging | `charging/is_charging` | 5 minutes | 5 min / 30 min / 2 h |
| Plugged | `charging/plugged` | 5 minutes | 5 min / 30 min / 2 h |
| Speed | `display/speed_kmh` | 1 active-driving minute plus one stop marker | averaged 5 min / 30 min / 2 h |
| Signed power | `charging/power_signed` | 1 active minute plus one zero marker | averaged 5 min / 30 min / 2 h |
| Odometer | `display/odometer_km` | changed values, at most once per core bucket | range forecast only |

The stored cadences are deployment parameters rather than fixed code constants.
Core and motion intervals can independently be 1, 5, 15, 30 or 60 minutes. The
pilot defaults are 5 minutes for SOC/charging/plugged and one minute for Speed and
signed power. Motion/power values are sampled only while non-zero. The first
transition to zero is stored immediately, including inside an already sampled
minute, and subsequent zeroes are suppressed. The API averages the available
minute samples into its fixed display resolution and also returns minimum and
maximum values for each output bucket.

The portal closes gaps in Speed and power series visually: after an overdue
sample it inserts zero, and one second before the next positive Speed value it
inserts zero again. This prevents linear interpolation from drawing a fictitious
slow drive across an offline/standstill interval. These markers are presentation
points only and do not create DynamoDB rows.

The three API ranges are 24 hours, 7 days and 30 days. Storage retention defaults
to 31 days and CloudFormation prevents a larger pilot value. DynamoDB TTL deletion
is asynchronous; expired records may remain briefly but the API time window never
returns data older than the requested range.

The original core signals have a fixed ceiling of 864 items per vehicle/day. The
deployed odometer addition can add at most 288 changed-value writes per
vehicle/day at the default cadence, but normally only writes while driving. Its
alarm threshold must therefore be reviewed as part of deployment. Speed and signed
power each add at most one write per active minute plus one zero marker per active
period; continuous zero values add no writes. A physically impossible 24-hour
upper bound is therefore about 3,744 items per vehicle/day plus transition markers. The deployed
daily alarm remains the operational cost guard and must be reviewed against
observed driving minutes. Duplicate moving publishes within a minute are rejected
before a history write using the previous value and timestamp returned by the
already-required state update.

## API and portal

`GET /api/vehicles/{vehicleId}/history?hours=24|168|720` uses the same Cognito JWT
authorizer and `has_active_access` check as snapshots. An inactive, unknown or
unassigned vehicle returns the same non-enumerating 404. Queries are partition-key
bound and eventually consistent.

The personal range forecast is deliberately decoupled from chart history through
`GET /api/vehicles/{vehicleId}/range-forecast`. The portal loads it once per
selected vehicle and keeps a forecast failure independent from chart rendering.
History range changes therefore perform only the six chart-signal queries and no
longer repeat the three raw 30-day forecast queries.

The portal draws the Display-CAN SOC and, when present, the separately labelled
internal Standard-CAN SOC in the same chart. It also draws averaged Speed,
averaged signed power, charging-state and plugged-state charts. Vehicle
selection reloads history. No history request contains a user-selected vehicle
without server-side authorization.

The separate range-forecast response returns `rangeForecast`. It is calculated
from raw 30-day SOC, changed-odometer and charging rows before chart aggregation.
At most ten recent valid journeys and roughly 150 km are used. The result contains
effective and historical kilometres per SOC point, confidence, total distance,
consumed SOC and journey count. Charging, tiny segments, odometer rollback and
implausible 50–300 km full-range equivalents are rejected. Until 100 km and 20 SOC
points have both accumulated, the historical rate is progressively blended with
the configured 1.4 km/SOC-point baseline.

## Cost controls and activation gate

- history is disabled by default;
- Speed requires a second explicit flag after the core-signal gate;
- an explicit allowlist bounds the pilot cohort; an empty list is fail-closed;
- DynamoDB uses pay per request, has no indexes and has PITR disabled for the MVP;
- the 31-day TTL and fixed sampling buckets bound storage growth;
- the History API accepts only three fixed windows and performs six partition
  queries; the independently loaded range forecast performs three bounded raw
  signal queries once per selected vehicle;
- `VehicleHistoryDailyWriteAlarm` defaults to 1,000 consumed writes/day;
- `tools/aws/measure_history_pilot.sh` reports daily ingest invocations plus
  history read/write capacity.

Before enabling the pilot, authenticate with AWS, record at least seven normal
days of state-ingest invocations, divide by active device-days, and explain any
outlier. Enable one vehicle cohort only after the measured publish rate is
consistent with the expected firmware cadence. Review the alarm and measured
table writes after 24 hours, 7 days and the first TTL boundary. Disable the flag if
the daily alarm breaches or if live snapshot/WebSocket regression tests fail.

## Measured baseline and deployment evidence

CloudWatch contained about 2.88 million state-ingest invocations across 15 daily
datapoints from 2026-07-15 through 2026-08-03. The narrower 2026-07-28 through
2026-08-04 hourly query contained 1,761,194 invocations across 119 active hourly
datapoints. Long plateaus were approximately 17,400 invocations/hour, or 4.83
invocations/second across all MQTT topics. This is the observed aggregate ingest
rate, not a per-signal history write rate.

The State table contained three vehicle partitions. They represent one physical
Microlino connected alternately to different telemetry devices, thereby simulating
different vehicle identities; they are not three simultaneously available cars.
`pioneer` was the only identity
with SOC, ODO and Speed state topics; its newest state was 2026-08-03 23:15:39 UTC.
`beta-02` last reported at 16:45:02 UTC and `beta-01` at 2026-07-28 15:03:50 UTC.

On 2026-08-04 the stack was first deployed with history disabled, an empty
allowlist and no ingest `PutItem` permission. Health returned 200, the history
table was empty and no new ingest errors appeared. The core pilot was then enabled
only for `pioneer`; motion signals remained disabled. The unauthenticated history
route returned 401 and the table remained empty because no new `pioneer`
telemetry had arrived after activation.

After the core gate, Speed was enabled for the same `pioneer` identity at
15-minute cadence. SOC and charging remain at 5-minute cadence. The update was
in-place with no resource replacement and reached `UPDATE_COMPLETE`; the table
remained empty because no device was online.

A short live check later on 2026-08-04 stored SOC 99 and SOC 100 in consecutive
five-minute buckets and Speed 0 in its motion bucket. Charging state was stored as
`false` in both core buckets although the vehicle was physically charging. This
proves the history cadence but leaves `charging/is_charging` decoder calibration
open for the controlled test. One ODO pilot row was written before ODO was removed
as low-value monotonic data; it is no longer queried and will expire by TTL.

`charging/plugged` was subsequently added without renaming or reinterpreting
`charging/is_charging`. This preserves the decoder-to-portal evidence chain while
showing cable detection separately. At default cadence the resulting 960-write
daily ceiling remains below the 1,000-write pilot alarm.

After the Node-RED legacy forwarder passed certificate, namespace and portal-user
setup, `xrpioneer` was added beside `pioneer` to the History allowlist. The
usage alarm was adjusted to 2,100 writes/day for the two-identity pilot. It
remains the active guardrail after the driving-minute refinement; an alarm breach
stops cohort expansion and triggers cadence/usage review.

After a real road test showed that 15-minute first-value Speed buckets were not
representative, the active-driving cadence was changed to one minute on
2026-08-04. The deployed ingest suppresses repeated standstill zeroes and writes
the first positive-to-zero transition immediately. The deployed API averages
Speed samples per fixed response bucket and returns the bucket maximum. Stack
`mot-aws-3-1` reached `UPDATE_COMPLETE`, runtime configuration reports 60 seconds,
both deployed Lambda packages contain the reviewed logic, Health returned 200,
the unauthenticated History route remained 401 and the 2,100-write alarm remained
`OK`. Hosted portal upload remains an operator action.

Cost Explorer access remains denied for the maintainer IAM user. Cost control is
therefore evidenced through bounded writes and CloudWatch usage, not a current
currency total. Billing access or exported billing evidence remains required.

## Known pilot limitations

- No backfill is attempted.
- TTL is a deletion mechanism, not an exact deletion timestamp.
- Road-test comparison suggests that `charging/plugged` may be inverted and that
  the current charging bit may need interpretation together with plugged/power.
  History deliberately preserves the raw decoded flags; correction belongs to a
  separately validated firmware/decoder work package.
- Core bucket values are first accepted values; Speed API buckets are arithmetic
  means of the available one-minute driving samples and include an immediate
  zero marker at journey end. Signed-power buckets use the same active-minute
  aggregation and preserve negative values.
- Currency-denominated Cost Explorer evidence is unavailable to the current IAM
user.

## GPS boundary

GPS remains live-only in this pilot. Persisting a useful OpenStreetMap history is
not just another scalar series: latitude and longitude must be paired, gaps and
stale fixes handled, location privacy retained and a route-capable map component
introduced. The existing live OpenStreetMap position remains unchanged. GPS
history should be a separate optional flag and cost/privacy decision if added.
