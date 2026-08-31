# RNG-SET-001 — Personal Range Settings

> **Status:** Active — backend deployed and hosted functional acceptance passed; cross-vehicle validation pending
>
> **Date:** 2026-08-24

## Objective

Replace the portal's implicit Pioneer-only 140 km assumption with two explicit,
authenticated settings for every user–vehicle association:

- full-range basis in kilometres at 100% SOC;
- desired SOC reserve at which displayed usable range reaches zero.

Both the fixed SOC comparison and the blended personal forecast use the same
settings. Existing users, vehicles, firmware and clients remain compatible through
defaults of 140 km and 0% reserve.

## Calculation contract

```text
usableSoc = max(0, currentSoc - reserveSoc)
fixedRangeKm = rangeKmAt100 * usableSoc / 100
personalRangeKm = blendedKmPerSoc * usableSoc

blendedKmPerSoc = confidence * historicalKmPerSoc
                + (1 - confidence) * rangeKmAt100 / 100
```

This removes the remaining Pioneer baseline from the rendered personal forecast
without changing the bounded History query or response contract. At or below the
selected reserve the displayed usable range is zero.

## Preference and compatibility contract

The existing authorized per-user/per-vehicle preference item gains:

- `rangeKmAt100`, integer 20–500, default 140;
- `rangeReserveSoc`, integer 0–50, default 0.

Old PUT clients preserve stored values when omitting the new fields. Older items
read with compatible defaults. The demo vehicle remains read-only. The firmware's
legacy `display/estimated_range_km` and fixed decoder value are not removed in
this slice; the portal does not use that value as its authoritative forecast.

## Portal slice

The two compact number fields appear in a distinct **Reichweite** group within the
existing settings card. A non-zero reserve is labelled `bis N%` in the range
display. The fixed SOC result remains visible as the transparent comparison.

## Deferred settings-page slice

As the surface now includes range, notification, email and SMS concerns, a
follow-up must move it to a dedicated authenticated settings page. That page must
preserve vehicle selection and include a clear button returning to the standard
dashboard page. This navigation/layout work is separate from the current additive
fields and persistence contract.

## Acceptance gates

- default 140 km / 0% reproduces existing displayed results;
- 140 km basis, 80% current SOC and 15% reserve yields 91 km fixed range;
- fixed and personal results reach zero at the selected reserve;
- changing vehicles reloads the matching association settings;
- old clients preserve both new fields;
- invalid values fail before persistence;
- focused API, portal-contract and JavaScript syntax tests pass;
- controlled AWS deployment and hosted desktop/smartphone acceptance remain
  separate operator gates.

## Deployment evidence

On 2026-08-24 the tested `preference_api.py` package was deployed only to the
existing `mot-dev-notification-preferences` Lambda. No CloudFormation resource,
role, table, route, SMS path or IoT rule changed. The function reached `Active`
with `LastUpdateStatus: Successful`; an unauthenticated direct probe returned the
expected application 401 without a function error. All 66 notification tests,
22 focused portal tests, JavaScript syntax validation and diff checks pass.

The maintainer uploaded the matching dashboard files and confirmed that both
values can be changed, remain stored and already affect the displayed range.
Validation with a second vehicle/battery profile and a full reload remains the
final functional breadth check.

## Related documents

- [JNY-001 journey summary and energy email pilot](JNY-001.md)
- [HIS-001 bounded telemetry history pilot](HIS-001.md)
- [Dashboard overview](../../dashboard/overview.md)
