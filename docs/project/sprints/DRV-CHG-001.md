# DRV-CHG-001 — Fahrstrecke und Reichweite seit letzter Ladung

**Status:** Active — backend deployed; portal upload and road acceptance pending

**Started:** 2026-09-13

## Objective

Extend the authenticated journey view with the current/last journey distance,
the latest vehicle odometer and a transparent SOC-based projection derived only
from driving observed since the last qualified charging session ended.

## Product contract

- Journey distance is `current or last odometer - journey start odometer`.
- The total odometer is the newest live value, with the journey endpoint value
  as a fallback. MOT does not synthesize an odometer when the signal is absent.
- A qualified charging completion stores only its end SOC, latest plausible
  odometer and completion timestamp in the existing charging session record.
- Unplug and the existing ten-minute charging-stop timeout establish the same
  reference. The odometer sample may precede charge start by at most 30 minutes.
- The portal shows distance and SOC development immediately, but calculates a
  projection only after at least 5 km and 5 consumed SOC percentage points.
- Projection to zero is `distance / SOC used * charge-end SOC`; projection to
  reserve uses the vehicle's configured SOC reserve.
- Missing, negative or inconsistent SOC/odometer differences fail closed and
  display no projection. A later qualified charge replaces the reference.

## Boundaries

- no firmware change or additional telemetry topic;
- no battery-capacity or SOC-equivalent energy calculation;
- no claim that the short-window projection is the long-term range forecast;
- no new table, public endpoint or authorization model;
- older session records remain valid and simply have no charging reference.

## Implementation state

The charging summary state now observes the existing odometer signal and retains
the last qualified charge reference across later sessions. Both unplug and the
delayed ten-minute completion path persist the reference with optimistic session
versioning. The existing JWT-protected `current-journey` response projects the
three reference values together with its existing journey odometer endpoints.

The DRV page adds journey distance, total odometer and a separate `Seit letzter
Ladung` card. German, English, French and Italian runtime wording is included.
The card explicitly withholds projection until its minimum evidence is reached.

## Deployment evidence — 2026-09-13

The tested code was deployed only to the existing `mot-dev-vehicle-api` and
`mot-dev-notifications` Lambda functions because a broad stack update would have
included unrelated drift. No IAM, table, route, scheduler or other resource was
changed. Both functions returned `Active` and `Successful` after deployment.
The Vehicle API's anonymous invocation still returns `401`; the Notification
Lambda's send-free invalid-topic probe returns the expected controlled rejection.

## Acceptance

- [x] repository state-machine tests for unplug and timeout reference handling;
- [x] endpoint contract returns no reference for legacy data and a minimized
  reference when present;
- [x] portal contract enforces 5 km / 5 SOC-point thresholds and configured reserve;
- [x] JavaScript and Python syntax checks;
- [x] isolated no-replacement AWS Lambda code deployment and smoke tests;
- [ ] hosted desktop and smartphone presentation;
- [ ] one qualified charge followed by sufficient driving validates the values.
