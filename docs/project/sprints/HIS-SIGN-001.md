# HIS-SIGN-001 — Signed Net Power History

> **Status:** Repository implementation complete; hosted acceptance pending
>
> **Date:** 2026-08-16

## Objective

Present historical net power with an unambiguous vehicle-facing sign convention:
consumption is negative and charging or regeneration is positive. Preserve the
stored signed values, existing History API, vehicle authorization, retention and
live telemetry paths.

## Scope

- invert only the portal representation of the existing signed History value;
- retain direction labels for consumption, charging and regeneration;
- draw a symmetric signed Y-axis with a visible zero line;
- keep reception-gap closure correct for both positive and negative samples;
- apply the shared portal behaviour to existing and future records;
- validate desktop and 390-pixel smartphone layouts.

Firmware, DynamoDB records, History aggregation and API response semantics are
unchanged. Existing records therefore require no migration.

## Controlled pilot identity

Read-only AWS verification on 2026-08-16 confirmed that `xrpioneer2` is present
in the deployed History allowlist. Its ACTIVE OWNER record references Cognito
subject `a07c897c-6031-7036-0757-e126bf3dc3d0`, which is the confirmed account
with email `xruser@microlino-open-telemetry.ch`.

## Acceptance evidence

- stored `+12.3 kW` consumption maps to displayed `−12.3 kW`;
- stored `−4.6 kW` battery intake maps to displayed `+4.6 kW`;
- zero is normalized to `0.0`, without negative zero;
- negative samples receive the same gap-closing zero marker as positive samples;
- the signed axis is symmetric around an emphasized zero line;
- local desktop 1440×900 and smartphone 390×844 visual checks passed without
  horizontal overflow;
- `node --check build/dashboard/current/js/history/history-chart.js` passed;
- 19 focused portal tests passed;
- scoped `git diff --check` passed.

## Remaining gate

Upload the shared portal package to the hosted dashboard and validate one existing
`xrpioneer2` 24-hour History view on desktop and smartphone.
