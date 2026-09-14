# VEH-TRIP-001 — Tripwerte seit letzter Ladung

**Status:** Accepted — backend deployed; multi-journey field test passed

**Prepared:** 2026-09-13

## Objective

Populate the existing `Trip`, `Verbrauch` and `Fahrzeit` fields in the main
dashboard Vehicle card with transparent cumulative driving values since the
latest qualified charging completion.

## Existing foundation

- DRV-CHG-001 already retains the qualified charge timestamp, SOC and real
  odometer and derives distance driven since that reference.
- JNY-001 already calculates distance, duration, drawn energy, recuperated
  energy and net energy for every qualified journey.
- The current-journey state already contains provisional distance and energy for
  an active journey.
- The existing authenticated current-journey API and main dashboard can carry
  additive nullable fields without a new public route.

The missing piece is a durable accumulator spanning multiple completed journeys
after one charging reference. Re-querying up to 31 days of History on every
dashboard refresh is explicitly not part of the design.

## Product contract

- **Trip** is the real odometer difference from the latest qualified charging
  completion to the newest plausible odometer. It is displayed in kilometres
  rounded to a whole kilometre because the retained display odometer supplies
  only whole-kilometre evidence. Internal calculation and qualification keep
  their full available precision.
- **Fahrzeit** is the sum of finalized, odometer-valid journey durations after that charging
  completion. While a journey is active, its current bounded duration is added
  provisionally.
- **Verbrauch** is cumulative net traction energy divided by cumulative Trip and
  is displayed as `kWh/100 km`. Drawn energy minus recuperated energy is used;
  it is not the gross drawn-energy value.
- The fields represent the same common period and never mix a newer charge
  reference with older journey totals.
- A newly qualified charging completion starts a new period. Repeated charging
  without driving moves the reference forward and leaves all three values at
  zero/unavailable until later driving.
- Active-journey additions are marked provisional internally and become final
  only through the existing journey finalization path. Reloading must not double
  count them.
- `Verbrauch` remains unavailable until both a positive distance and usable
  energy evidence exist. Missing or contradictory odometer/energy evidence
  fails closed rather than being estimated.
- Legacy records show `--` until a later qualified charging completion creates
  the new aggregate state.
- German, English, French and Italian labels, units and number formatting follow
  the selected portal language.

## Implementation state

The isolated Journey item now carries a versioned `driveSinceCharge` map with the
charge-reference identity, last finalized journey ID, journey count, duration
and drawn/recuperated/net energy totals. Journey finalization writes the cleared
Journey state and the idempotently updated aggregate in one optimistic write.
Short journeys excluded from notification email remain included when their real
odometer and energy evidence are usable.

A newer qualified charge reference logically resets the period immediately: the
API exposes no totals whose stored reference identity differs. The next finalized
journey replaces the old aggregate. This avoids a cross-item transaction between
the independently serialized Charging and Journey state machines.

The protected existing `current-journey` response projects finalized totals plus
the active journey without persisting the provisional addition. Distance remains
the real odometer delta from the charge reference. The main dashboard loads the
same response once per minute and renders the existing three Vehicle-card fields
on desktop and smartphone, clearing them on vehicle changes.

## Boundaries

- no firmware or telemetry-topic change;
- no diagnostic table or debug data;
- no new DynamoDB table, public endpoint or authorization model;
- no reconstruction or migration of historic periods before the next qualified
  charge;
- no replacement of the separate previous-month Fleet Efficiency comparison;
- no claim of billing-grade or precision energy measurement.

## Acceptance plan

- pure state tests cover reset on charge, several journeys, recuperation,
  duplicate finalization and active-to-final transition;
- API tests cover legacy/null state, minimized projection and authorization;
- portal tests cover units, four languages, vehicle switching and responsive
  presentation;
- regression suites for notifications, Foundation API and dashboard pass;
- productive test sequence: qualified charge, two drives separated by a stop,
  reload between drives, then a second qualified charge proving reset;
- AWS changes, if needed, use the established reviewed code-only deployment of
  the existing Notification and Vehicle API Lambdas without replacement.

## Field acceptance — 2026-09-14

The hosted dashboard correctly accumulated several journeys after one charge
and retained the period across reloads. The calculation also remained withheld
until the existing 5 km qualification boundary was reached. The maintainer
accepted that behaviour. A presentation-only follow-up rounds dashboard Trip,
journey/odometer distances and journey/daily notification distances to whole
kilometres; thresholds, aggregation and stored evidence remain unchanged.

## Backend deployment — 2026-09-13

After explicit maintainer approval, the established code-only deployment updated
only the existing `mot-dev-vehicle-api` and `mot-dev-notifications` functions in
AWS account `002581114110`, region `eu-north-1`. No CloudFormation stack, table,
route, IAM policy or stored record was changed. Both functions returned `Active`
and `LastUpdateStatus: Successful`; their new code hashes are
`eYX8dNPnfNXLaTRvQRRa5E5qvGOj/jZ5h/Cx/VILnzA=` and
`epOKF+42gdeSMuBJ/dQOJMgoUSMXT93bVyno/e0re5E=` respectively.

An anonymous current-journey invocation retained the expected application `401`.
The Notification Lambda rejected an irrelevant topic with `invalid_topic`,
without a delivery path. Repository validation passed 124 notification, 36
Foundation/API and 37 focused portal tests plus Python/JavaScript syntax and
whitespace checks.

The first productive 5.1 km drive exposed a compatibility edge in a pre-existing
charge balance: its reference was finalized 75 seconds after movement began, so
the initial strict `journey.started_at >= reference_at` check omitted the
aggregate although the odometer-based Trip was correct. The bounded acceptance
now permits only the existing two-minute movement-confirmation window and has
dedicated state/API regression coverage. The already recorded journey event
(7 min, 0.417 kWh drawn, 0.094 kWh recuperated, 0.323 kWh net) was copied once,
without estimation, into the previously absent `pioneer` aggregate using a
conditional DynamoDB update. Later journeys use the normal finalization path.
