# CHG-BAL-001 — Ladebilanz zwischen Fahrten

> **Status:** Backend deployed; portal upload and field acceptance open
>
> **Started:** 2026-09-13

## Objective

Replace the dashboard's single-session charging snapshot with a cumulative charge
balance that follows Microlino behaviour across charging pauses, unplug/replug,
automatic top-ups and measurable standby discharge. Keep notification email
semantics and delivery idempotency unchanged.

## Product contract

- `chargingSummary` remains the email-only, individually qualified charging
  session. Its 45-second qualification, unplug/ten-minute completion, event and
  delivery identifiers do not change.
- A separate `chargingDisplay` state opens after the first 45-second-qualified
  charge and remains open across any number of charging sessions and pauses.
- Signed standard telemetry accumulates charging energy as gross input and
  positive battery draw as measurable pre-drive discharge. Net energy is gross
  minus discharge. Gaps longer than 30 seconds are not invented.
- The SOC immediately preceding the first charging assertion is retained rather
  than replacing it after the qualification delay. The balance also retains peak
  SOC and SOC observed when the next drive begins.
- A real odometer increase starts a 60-second movement confirmation and then
  closes the display block. If odometer is unavailable, speed above 1 km/h across
  the same window is the bounded fallback. This window retains delayed SOC
  corrections after wake-up; ignition without movement does not close the block.
- The protected current-journey API prefers the new current/final display balance
  and falls back unchanged to legacy `chargingSummary` snapshots.
- The dashboard shows total charged, measurable discharge, net battery input, SOC
  progression, charging-session count, coverage and whether the balance is still
  open. It never reads diagnostic storage.
- While the balance is open, the journey card is headed `Aktueller Ladeblock`
  and uses its start/current SOC. Distance and range projection from the preceding
  cycle are withheld to avoid mixing two reference periods. After confirmed
  movement it returns to `Seit letzter Ladung` with the finalized reference.

## Compatibility and storage

The new state is an additive `chargingDisplay` map in the existing per-vehicle
notification session item. No table, route, IAM, firmware topic, user preference
or migration is introduced. Existing records remain readable and acquire the new
map only on later relevant telemetry. Email dispatch continues to read only the
existing `chargingSummary` state.

## Validation

- pure state tests cover multiple sessions, intervening discharge, SOC peak/end,
  odometer and speed finalization, unqualified-charge rejection and signed-power
  zero crossing;
- Foundation API tests cover legacy fallback and preferred finalized balance;
- dashboard contracts cover all additive fields, cache versions and four-language
  wording;
- the open-block journey presentation and its deliberate suppression of stale
  distance/projection values are covered by the focused portal contract;
- full notification, Foundation and focused dashboard regression suites pass;
- natural multi-session vehicle acceptance remains open.

## Backend deployment — 2026-09-13

Following explicit maintainer approval, the existing code-only deployment route
updated only `mot-dev-vehicle-api` and `mot-dev-notifications`. No CloudFormation
stack, table, IAM policy, user/preference record, email setting or portal asset was
changed. Both functions returned `Active` and `Successful`; their deployed code
hashes are `+4byV4cmp5HqAKkTduTwaHGKAw3JinpobAuNjx7SzMo=` and
`RuxQ155ipRYvGG2I6pCXik2QMM2UujTEwag8h2ctLlM=` respectively.

A send-free invalid-topic invocation returned the expected controlled
`invalid_topic` rejection. The existing `pioneer` email charging state remained
intact and had not yet acquired `chargingDisplay`, which is the intended additive
legacy behaviour: the map is created only by subsequent relevant charge
telemetry. Portal upload and a natural repeated-charge/discharge/drive sequence
remain the acceptance gates.

## First physical energy reference — `pioneer`

The first post-deployment block charged from 84 percent to a 100-percent peak and
remained at 100 percent. MOT accumulated 1.4829 kWh gross, 0.0023 kWh measurable
pre-drive discharge and 1.4806 kWh net across two detected charging segments. Its
block-wide power coverage was 74.4 percent, so the UI correctly presents the
gross value as a minimum. Independent references supplied by the maintainer were
1.67 kWh at the mobile charger and 1.76 kWh at the AC grid meter. MOT therefore
captured 88.8 percent of the charger value and 84.3 percent of the grid value;
the 1.68 kWh SOC/capacity estimate was within 0.01 kWh of the charger reading.

The 0.09 kWh difference between grid meter and mobile charger is outside MOT's
pack-side measurement and plausibly represents charger/cable conversion and
auxiliary losses. This single observation validates the conservative minimum and
separate-estimate presentation, but is not yet sufficient to calibrate a general
loss factor. The display block remains open until the next confirmed drive.
