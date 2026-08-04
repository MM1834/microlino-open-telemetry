# Telemetry History Pilot

> **Status:** Core pilot deployed for `pioneer`; motion signals remain disabled
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
| Charging | `charging/is_charging` | 5 minutes | 5 min / 30 min / 2 h |
| Plugged | `charging/plugged` | 5 minutes | 5 min / 30 min / 2 h |
| Speed | `display/speed_kmh` | 15 minutes | 5 min / 30 min / 2 h |

The stored cadences are deployment parameters rather than fixed code constants.
Core and motion intervals can independently be 1, 5, 15, 30 or 60 minutes. The
pilot defaults remain 5 minutes for SOC/charging/plugged and 15 minutes for Speed.
Shorter settings increase the documented write ceiling and require a matching
alarm-threshold review before deployment.

The three API ranges are 24 hours, 7 days and 30 days. Storage retention defaults
to 31 days and CloudFormation prevents a larger pilot value. DynamoDB TTL deletion
is asynchronous; expired records may remain briefly but the API time window never
returns data older than the requested range.

At full publication availability the write ceiling is 960 items per vehicle/day:
three signals × 288 five-minute buckets plus one signal × 96 fifteen-minute
buckets. Duplicate publishes within a bucket are rejected before a history write
using the previous timestamp returned by the already-required state update.

## API and portal

`GET /api/vehicles/{vehicleId}/history?hours=24|168|720` uses the same Cognito JWT
authorizer and `has_active_access` check as snapshots. An inactive, unknown or
unassigned vehicle returns the same non-enumerating 404. Queries are partition-key
bound and eventually consistent.

The portal draws SOC, Speed, charging-state and plugged-state charts. Vehicle
selection reloads history. No history request contains a user-selected vehicle
without server-side authorization.

## Cost controls and activation gate

- history is disabled by default;
- Speed requires a second explicit flag after the core-signal gate;
- an explicit allowlist bounds the pilot cohort; an empty list is fail-closed;
- DynamoDB uses pay per request, has no indexes and has PITR disabled for the MVP;
- the 31-day TTL and fixed sampling buckets bound storage growth;
- the API accepts only three fixed windows and performs four partition queries;
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
two-identity default maximum is 1,920 writes/day, so the usage alarm was adjusted
to 2,100 writes/day. This changes neither cadence nor retention.

Cost Explorer access remains denied for the maintainer IAM user. Cost control is
therefore evidenced through bounded writes and CloudWatch usage, not a current
currency total. Billing access or exported billing evidence remains required.

## Known pilot limitations

- No backfill is attempted.
- TTL is a deletion mechanism, not an exact deletion timestamp.
- Bucket values are first accepted values, not statistical aggregates.
- Currency-denominated Cost Explorer evidence is unavailable to the current IAM
user.

## GPS boundary

GPS remains live-only in this pilot. Persisting a useful OpenStreetMap history is
not just another scalar series: latitude and longitude must be paired, gaps and
stale fixes handled, location privacy retained and a route-capable map component
introduced. The existing live OpenStreetMap position remains unchanged. GPS
history should be a separate optional flag and cost/privacy decision if added.
