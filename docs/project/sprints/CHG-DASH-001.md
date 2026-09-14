# CHG-DASH-001 — Letzte Ladeenergie im Dashboard

> **Status:** Backend deployed; MOT beta portal uploaded; natural acceptance open
>
> **Started:** 2026-09-13

## Objective

Show the measured energy from the last completed qualified charge in the existing
charging box on the main dashboard and in the adjusted journey view. Preserve the
CHG-COV-001 distinction between measured minimum energy and the separate SOC-based
estimate.

## Product contract

- The main charging box labels the value `Letzte Ladung` rather than implying a
  live session counter.
- Both main and journey views show the same completed charging record.
- At power-data coverage below 95 percent, measured energy is prefixed with
  `mind.`.
- SOC-estimated energy and power-data coverage are shown as secondary context when
  available.
- A completion timestamp is shown on the main dashboard.
- Legacy records fail closed with `Noch keine abgeschlossene Ladung`.
- Vehicle changes clear the previous vehicle's value before requesting the new
  one. The main dashboard refreshes the reference every 60 seconds.
- German, English, French and Italian wording is included.

## Data path

The notification state retains an additive snapshot when a qualified charging
session completes by unplug or by the existing ten-minute stopped-charging path:

- measured pack-side energy;
- completion timestamp;
- power-data coverage;
- SOC-based energy estimate, when a declared capacity and valid SOC delta exist;
- charge-start and charge-end SOC context.

The existing JWT-protected `current-journey` route returns this as `lastCharge`.
No new route, table, authorization rule, diagnostic query or firmware topic is
introduced. The existing `chargeReference` used by DRV-CHG-001 remains independent
and continues to require a plausible odometer anchor.

## Repository implementation

- `charging_summary_state.py` preserves the last completed energy-quality
  snapshot across later sessions.
- Both unplug and delayed completion enrich the snapshot using the already
  authorized vehicle-capacity profile.
- The Vehicle API adds a minimized nullable `lastCharge` object to the existing
  response.
- The main dashboard fills its previously unused energy field and the journey
  view adds the same evidence to `Seit letzter Ladung`.

## Validation

- 110 notification tests pass.
- 30 Foundation authorization/API tests pass.
- 47 focused dashboard tests pass after updating the intentional asset cache
  versions.
- JavaScript and Python syntax checks pass.
- Both CloudFormation templates validate in AWS.
- Local browser inspection confirms the existing main charging-box layout and
  all four language catalogs; authenticated data presentation remains a hosted
  acceptance item.

## Deployment boundary

On 2026-09-13 the maintainer uploaded the frontend to the MOT beta portal and
explicitly approved the backend rollout. The existing code-only deployment route
updated only `mot-dev-vehicle-api` and `mot-dev-notifications`; it made no stack,
table, IAM, user-assignment or stored journey-data change. Both functions returned
`Active` and `Successful` after deployment, with code hashes
`fyrI0VoD8sgwHPQV/K86lJycIrgr0rltOGzAAZxGOyo=` and
`suG7e60e7aVDI6/Hr+ufHG8iBoV20V96yyAIsPXeYNw=` respectively.

The first pre-deployment physical probe was already sufficient to qualify: the
controlled `pioneer` charge ran for six minutes from 96 to 98 percent, integrated
0.2075 kWh from 65 samples and had 83.8 percent coverage. It correctly dispatched
its email, but could not populate the new snapshot because the backend code was
not yet live. The subsequent active session is preserved and can populate the
snapshot naturally when it completes. Productive acceptance still requires a
completed post-deployment charge, checked in both dashboard views; a later
good-coverage charge should also verify presentation without the `mind.` prefix.

The first MOT beta inspection exposed a presentation-only journey defect:
JavaScript converted a missing journey start odometer (`null`) to numeric zero,
so `Gefahrene Strecke` repeated the full vehicle odometer. The drive view now
treats null, undefined and empty numeric inputs as unavailable and therefore
shows `-- km` until both real endpoints exist. The charge-reference date also
includes a two-digit year. The drive asset version was advanced to
`20260913-distance-fix1`; the main dashboard is unchanged.
