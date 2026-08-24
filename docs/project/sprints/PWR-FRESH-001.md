# PWR-FRESH-001 — Charging and Power Freshness

> **Status:** Complete — hosted desktop and smartphone acceptance passed
>
> **Opened:** 2026-08-24

## Objective

Make retained charging and power values visibly distinguishable from current
telemetry in the adaptive overview card and net-power History on desktop and
smartphone.

## Scope

- use each relevant topic's authoritative `receivedAt` timestamp;
- evaluate charging/plugged freshness for the stationary charging view;
- evaluate vehicle/pack/compatibility power freshness for the driving view;
- evaluate the newest applicable charging or power update while charging;
- use the existing 120-second vehicle freshness boundary;
- dim only the adaptive charging/power contents when stale;
- show `Nicht aktuell · letzter Messpunkt hh:mm` without changing the retained value;
- clear stale presentation on vehicle changes and fresh telemetry;
- annotate net-power History from its last real measurement before synthetic gap
  closing points are added.

## Acceptance gates

- [x] Current charging and power data retain the existing presentation.
- [x] Stale data are dimmed and carry a local-time last-measurement note.
- [x] Stationary, charging and driving modes select their relevant topic group.
- [x] The check is reevaluated continuously and on snapshot/live updates.
- [x] Focused source contracts and JavaScript syntax validation pass.
- [x] Net-power History reports current/stale state and its last real measurement.
- [x] Hosted desktop acceptance passes with current and stale data.
- [x] Hosted smartphone acceptance passes with current and stale data.

## Boundary

This sprint changes only presentation freshness. It does not replace retained
values with zero, infer a new charging state, modify telemetry cadence or change
AWS storage/API semantics.

## Validation evidence

The complete portal/tool suite passes with 185 tests, both modified JavaScript
files pass syntax validation and the repository diff is whitespace-clean. The
hosted files were then accepted on desktop and smartphone on 2026-08-24. The
maintainer verified the adaptive overview presentation and the net-power History
freshness marker; PWR-FRESH-001 is closed.
