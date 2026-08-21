# HIS-BIN-001 — Combined Binary Charging History

> **Status:** Complete — hosted desktop/smartphone acceptance passed
>
> **Opened:** 2026-08-21

## Objective

Combine the separate `Ladezustand` and `Ladekabel` History panels into one compact
binary chart while making their relationship readable and preventing misleading
diagonal interpolation between discrete states.

## Scope

- render `Lädt` and `Kabel angeschlossen` on the same zero/one axis;
- use a solid purple charging line and a dashed pink plugged line;
- label the axis as `Aus` and `Ein` rather than repeated rounded numbers;
- hold each known state horizontally until the next known state;
- change state only with a vertical edge at the new timestamp;
- preserve missing-field handling, History ranges and source/API semantics;
- remove the redundant fifth History card without changing backend data.
- prevent the five-second vehicle poll from repeatedly reloading History;
- retain the last successful charts when an automatic History refresh fails;
- give the Vehicle API sufficient memory and timeout headroom for 30-day queries.

## Acceptance gates

- [x] One combined chart replaces the two separate cards.
- [x] Both series remain distinguishable when charging and plugged overlap.
- [x] Missing samples cannot create a diagonal binary transition.
- [x] Focused contract tests cover chart composition and step geometry.
- [x] Responsive desktop and smartphone visual acceptance passes locally.
- [x] Hosted combined-chart desktop and smartphone acceptance passes.
- [x] Automatic History refresh retains the selected 24h/7d/30d range locally.
- [x] Hosted 7-day automatic-refresh regression acceptance passes.
- [x] Concurrent same-range loads are coalesced and stale responses cannot overwrite a newer range.
- [x] A failed refresh leaves the last successful chart visible and reports a discreet warning.
- [x] The Vehicle API 30-day path has 256 MB memory and a 25-second timeout.
- [x] Live Vehicle API capacity update returns a real 30-day request successfully.
- [x] Hosted 30-day browser load and refresh acceptance passes.

## Boundary

The chart does not infer plugged state from charging, rewrite stored History or
change the History API contract. Each series continues to use its own reported
values. The follow-up changes only Vehicle API runtime capacity and portal request
scheduling/error handling; stored data and response semantics remain unchanged.

## Validation evidence

The focused contract tests, complete portal/tool test suite and JavaScript syntax
check pass. A deterministic fixture with separated cable and charging transitions
was rendered at 390 × 844 and 1440 × 900. Both views showed only horizontal holds
and vertical edges, retained the solid/dashed distinction during overlap and had
no horizontal overflow. The fixture was removed after validation.

Hosted desktop and smartphone acceptance of the combined chart passed on
2026-08-21. That observation exposed a separate refresh defect: external
`render()` calls used the former 24-hour default while leaving the 7-day button
active. `render()` now defaults to the current range, validates it against the
allowed range set and synchronizes the active button on every render. A browser
regression selected 7d, triggered the same parameterless automatic refresh and
retained both `letzte 7d` in the rendered metadata and the active 7d button.

Hosted 7-day acceptance passed on 2026-08-21. The subsequent 30-day test exposed
intermittently empty charts after data had briefly rendered. CloudWatch evidence
showed multiple Vehicle API invocations reaching the exact 10-second Lambda limit,
plus errors and account-level throttles during the same window. The portal also
reloaded the full selected History range from every five-second vehicle-list poll
and converted every failed refresh into an empty dataset. The fix removes that
poll-driven History reload, coalesces overlapping requests for the same vehicle and
range, rejects stale completions, and retains already rendered data on failure.
The Vehicle API runtime is raised from 128 to 256 MB and from 10 to 25 seconds.

The live capacity update completed successfully on 2026-08-21. A direct,
authorized 30-day request for `xrpioneer2` returned HTTP 200 with 51 points at
two-hour resolution. Its Lambda duration was 4.995 seconds at 256 MB and maximum
memory use was 119 MB; immediately preceding production requests had repeatedly
reached the former exact 10-second timeout at 128 MB. Hosted testing subsequently
passed with different range combinations and users on desktop and smartphone.
The deployment script now packages the oversized template through the dedicated
artifact bucket and no longer injects stack tags into untagged legacy resources.
A reviewed Change Set modified only Vehicle API capacity and its dependent
integration, without replacement; stack `mot-aws-3-1` reached `UPDATE_COMPLETE`
and runtime verification reports 256 MB, 25 seconds, `Active` and `Successful`.
