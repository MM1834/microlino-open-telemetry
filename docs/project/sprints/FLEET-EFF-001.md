# FLEET-EFF-001 - Anonymized Long-Term Fleet Efficiency

> **Status:** Active - monthly backend and 190-journey backfill deployed
>
> **Date:** 2026-09-10

## Objective

Retain a privacy-minimized monthly efficiency record beyond the existing
31-day journey-event TTL. The result must support a defensible multi-month
fleet statement for distance and battery-side net energy without retaining
users, contact addresses, vehicle identities, routes or individual journeys in
the long-term aggregate.

The current August/September evidence must be backfilled before the oldest
source events become eligible for TTL deletion around 2026-09-15.

## Current boundary

`mot-dev-notification-events` retains journey summaries for at most 31 days.
Those records are created only for users with confirmed email and an enabled
journey or daily summary. They are therefore useful pilot evidence but are not
a complete fleet-analytics population.

One completed journey can also produce more than one user-scoped notification
event. Long-term aggregation must not sum those records directly. Its canonical
idempotency key is the logical `vehicleId + journeyId`, independent of recipients
and delivery channels.

## Long-term aggregate

Store one encrypted, on-demand monthly aggregate per environment. A record may
contain only:

- Zurich calendar month and schema version;
- qualified journey count;
- total distance;
- total drawn, regenerated and net battery energy;
- total consumed SOC percentage points;
- capacity-weighted SOC-equivalent energy, but only for journeys with an
  accepted vehicle-capacity classification;
- counts included in and excluded from the SOC comparison;
- firmware-counter and telemetry-estimate counts;
- finalized active-vehicle count, if it can be derived with a short-lived
  deduplication record that is deleted after month close;
- creation, last-update and finalization timestamps.

Do not retain a long-term capacity-class breakdown while a class contains only
one or very few vehicles. The aggregate can add each accepted journey's
SOC-equivalent energy at ingestion time and publish only the combined fleet
result. No email, Cognito subject, vehicle ID, adapter ID, VIN, location,
journey ID or raw telemetry belongs in the durable monthly item.

The monthly record may be retained for at least 36 months. Individual
idempotency markers remain short-lived and contain no user/contact data.

## Vehicle battery-capacity profile

Add canonical vehicle metadata outside user preferences and adapter settings:

```text
batteryCapacityKwh
batteryCapacitySource
batteryCapacityDeclaredAt
batteryCapacityVerificationStatus
batteryCapacityVerifiedAt
schemaVersion
```

The capacity belongs to the logical vehicle profile. It must survive user claim
changes and adapter replacement. Initial entry is made during administrator
vehicle provisioning or claim/onboarding and requires explicit confirmation.
CAN profile selection may suggest a value but must never silently establish it.

Because MOT does not currently read or bind a VIN/FIN, `vehicleId` is not proof
of a physical vehicle. Onboarding must state this limitation and require a new
capacity confirmation whenever an adapter is assigned to another vehicle. A
future validated VIN/FIN signal may strengthen the association but is not a
dependency and must not be collected without a separate privacy decision.

## Capacity plausibility control

Capacity verification is advisory and must not overwrite the declared value.
Use several qualified observations, not one journey, to compare the declared
capacity with independent evidence such as:

- net journey energy divided by consumed SOC fraction;
- charging energy divided by gained SOC fraction where session completeness is
  sufficient;
- consistency across multiple journeys or charging sessions;
- discontinuities after adapter reassignment or a material profile change.

Whole-percent SOC quantization, auxiliary loads, charging losses and incomplete
telemetry require broad, documented tolerances. The exact sample minimum and
tolerance are acceptance evidence to be calibrated from pilot data, not guessed
constants in the first implementation.

Use explicit states such as `UNKNOWN`, `DECLARED`, `PLAUSIBLE` and `CONFLICT`.
Missing capacity, insufficient evidence or a conflict excludes that journey from
the SOC-equivalent comparison. Distance and directly measured net-energy totals
remain valid and must continue to be aggregated. Never infer or silently change
the battery capacity solely to make the SOC comparison agree.

## Consent and data-use boundary

Fleet analytics is independent of email delivery. Before production go-live it
requires an explicit, versioned onboarding disclosure and consent/policy
decision covering purpose, fields, aggregation, retention, withdrawal and the
controller/contact path. The implementation must define what happens after
withdrawal and must not claim that a small cohort is anonymous when it remains
reasonably linkable.

Until that decision is accepted, the implementation is a controlled pilot using
the already authorized journey-summary population. It must label coverage and
must not describe the result as all vehicle activity.

## Optional personal community comparison

Offer an explicitly optional portal result titled `Deine Fahreffizienz im
Vergleich zur Community`. It compares only battery-side driving efficiency and
must never include SOC, battery capacity or an SOC-equivalent result.

The presentation contains:

- the user's distance, net energy and kWh/100 km for the comparison month;
- the community distance, net energy and kWh/100 km for the same month;
- the absolute and percentage difference in kWh/100 km;
- one compact flag: `+` for materially more efficient, `=` for the accepted
  neutral tolerance and `-` for materially less efficient;
- the month, contributing journey count and whether the month is finalized or
  still provisional.

Lower kWh/100 km is more efficient. The neutral tolerance must be documented and
validated before release; do not derive a positive or negative flag from
rounding noise. Prefer the previous finalized Zurich calendar month. A current
month may be shown only as explicitly provisional and must use the same cutoff
for personal and community values.

The community comparator should exclude the requesting user's own contribution
where the data model can do so safely. Suppress the comparison when the remaining
cohort is below an accepted privacy/sample threshold or has insufficient distance
or journeys. Never expose capacity-class or individual-vehicle breakdowns.

This feature uses a separate private, JWT-authorized monthly user total. That
record is pseudonymous personal data and is not part of the anonymous fleet
table. During the MOT pilot it follows the existing authorized journey-summary
population. Define its retention, deletion on account withdrawal and onboarding
consent before the post-pilot production go-live. Community totals remain
anonymous; the browser receives no other user's contribution.

## Delivery slices

| Slice | Outcome | Status |
|---|---|---|
| FLEET-EFF-001.A | Backfill current qualified events once per logical journey into reviewed monthly aggregates | Deployed and validated - 190 journeys |
| FLEET-EFF-001.B | Canonical vehicle profile with declared battery capacity, provenance and audit metadata | Backend table and controlled initial declarations deployed; onboarding integration planned |
| FLEET-EFF-001.C | Conservative multi-sample capacity plausibility state and SOC-comparison exclusion | Planned |
| FLEET-EFF-001.D | Idempotent per-journey monthly aggregation independent of notification recipients | Deployed and naturally validated with three subsequent journeys |
| FLEET-EFF-001.E | Versioned onboarding disclosure/consent and withdrawal behaviour | Blocked on privacy-policy decision |
| FLEET-EFF-001.F | Monthly close, export/report and 36-month retention validation | Planned |
| FLEET-EFF-001.G | Optional private `+ / = / -` personal-versus-community efficiency comparison, without SOC | Complete - backend deployed and hosted desktop/smartphone accepted |

## Acceptance criteria

- Backfill reproduces the reviewed aggregate for the same cutoff and records its
  exact source window and count.
- A shared vehicle journey is counted once regardless of the number of users or
  notification subscriptions.
- Retries and out-of-order delivery cannot increment a month twice.
- Missing or conflicting battery capacity cannot contribute SOC-equivalent
  energy.
- Direct distance/net-energy statistics remain available when SOC comparison is
  excluded.
- Long-term items contain no direct or pseudonymous user, vehicle, adapter,
  journey or location identifier.
- 10.5 kWh and 15 kWh controlled examples produce the correct combined
  capacity-weighted SOC energy without publishing a small capacity cohort.
- Retention, encryption, IAM, alarms, cost estimate, rollback and deletion
  behaviour are reviewed before deployment.
- Production onboarding cannot enable fleet analytics before the versioned
  data-use disclosure and consent/policy gate is accepted.
- Personal comparison shows both absolute kWh/100 km values and the difference,
  uses the same closed-month window, excludes the requesting user's contribution
  and emits no SOC or battery-capacity value.
- Small or insufficient community cohorts return `not enough data` instead of a
  flag or potentially identifying comparison.

## Cost boundary

Use DynamoDB on-demand capacity and atomic increments. Normal operation adds at
most one aggregate update plus one short-lived deduplication decision per
qualified logical journey. At pilot volume, storage is a few monthly records and
cost should remain negligible; deployment evidence must still record actual
write and Lambda metrics.

## Deployment evidence - 2026-09-10

The repository adds an encrypted, PITR-enabled monthly aggregate table, an
encrypted PITR-enabled canonical vehicle-profile table and a 93-day TTL marker
table. `NEW_IMAGE` streaming on the existing notification-event table invokes a
separate 128 MiB Lambda. Its role can read one vehicle profile, consume only that
stream and update only the marker and monthly tables. Existing notification
delivery code and tables are not part of the aggregation transaction.

Reviewed Change Set `fleet-eff-001-a-narrow-20260910` reached
`UPDATE_COMPLETE`. It added the three tables, Lambda, role, stream mapping and
log group and enabled the existing table stream without replacing a table or
function. PITR is enabled on the durable tables, marker TTL reports `ENABLED`,
the Lambda is `Active`/`Successful` and the event source mapping is enabled.

The controlled backfill resolved the maintainer-declared 15 kWh profile for
exactly one vehicle and 10.5 kWh for the other six without storing email in the
profile. It wrote seven `DECLARED` profiles and 190 hashed short-lived markers.
The two durable anonymous results are:

| Month | Journeys | Distance | Net energy | SOC-equivalent energy | Firmware / telemetry |
|---|---:|---:|---:|---:|---:|
| 2026-08 | 118 | 2,828.6 km | 208.457 kWh | 208.725 kWh | 0 / 118 |
| 2026-09 through deployment | 72 | 1,422.7 km | 105.010 kWh | 106.680 kWh | 33 / 39 |

A complete second backfill recorded zero and classified all 190 inputs as
duplicates while leaving both monthly items unchanged. A direct deployed-Lambda
smoke test with an already recorded stream image returned one duplicate, zero
new records and zero ignored records. CloudWatch contained no error event. All
96 focused notification tests, Python syntax, whitespace validation and AWS
CloudFormation validation pass. A naturally created post-deployment journey is
the remaining live-stream acceptance gate.

## FLEET-EFF-001.G deployment evidence - 2026-09-10

The monthly backend now stores `averageNetKwhPer100Km` when the preceding Zurich
calendar month is finalized at 00:15 on its first day. The dashboard API reads
that stored community average and the stored private vehicle average. It performs
only the request-specific leave-one-out subtraction because each user's community
baseline necessarily differs. No SOC or battery-capacity field is returned.

The private table uses `month + SHA-256(userSub|vehicleId)` and contains only
journey count, distance, net energy, stored average, finalization metadata and a
400-day TTL. It is encrypted, PITR-enabled and readable only by the separate
comparison Lambda after the existing active vehicle authorization succeeds.
The community comparison requires at least three other active vehicles, ten
other journeys and 100 other kilometres. A fixed 5 percent neutral band maps
lower personal consumption to `+`, the neutral band to `=` and higher consumption
to `-`.

Reviewed Change Set `fleet-eff-001-g-20260910` added the private table, JWT route,
comparison Lambda/role and Zurich monthly finalizer without replacement or
deletion. The subsequent in-place code Change Set
`fleet-eff-001-g-month-fix-20260910` corrected the tested first-day-at-midnight
Vormonat boundary. August was finalized with one stored community average and
six private vehicle averages. Productive direct validation returned six available
comparisons (two `+`, one `=`, three `-`), the exact August period and no SOC,
battery or identity field. Anonymous HTTP access returns 401, private TTL and
PITR are enabled, the scheduler is enabled and comparison error logs are empty.
All 102 focused backend tests, 10 focused dashboard/i18n tests and JavaScript
syntax checks pass. Hosted desktop and smartphone acceptance passed on
2026-09-10. A final shared-file review confirmed that the efficiency card/API
and the parallel DRV-001 route/page coexist; aligned cache-busting identifiers
ensure that all dashboard surfaces load the combined shared assets.
