# CHG-SUM-001 — Email Charging Summary

> **Status:** Backend deployed; preference migration complete; hosted portal acceptance open
>
> **Started:** 2026-09-01

## Objective

Offer an optional email-only summary for completed charging sessions without
reading the diagnostic capture path. Existing users who already enabled both
email notifications and journey summaries receive the new option automatically.

## Accepted behaviour

- The summary reads only the normal notification stream: Display-CAN
  `display/soc`, `charging/*`, `bms/vehicle_power_w` and the existing
  `charging/power_signed` fallback.
- Diagnostic history and debug-table data are never queried.
- Charging must remain active for 45 seconds before a session qualifies.
- The session completes immediately on unplug, or after ten continuous minutes
  without charging. A restart inside that window cancels completion.
- Delivery is email-only and idempotent per plugged session.
- The email includes start/end Display-CAN SOC, SOC change, duration, estimated
  charged energy and the completion reason.

## Deployment evidence

CloudFormation stack `mot-dev-notifications` reached `UPDATE_COMPLETE` on
2026-09-01. All three Lambda resources report `UPDATE_COMPLETE`; the update did
not replace a data-bearing resource. The preference migration selected records
with both `emailEnabled=true` and `journeyEmailEnabled=true`: 9 of 9 matching
records now have `chargingSummaryEmailEnabled=true`, and no non-matching record
was enabled.

The repository dashboard contains the optional checkbox in German, English and
French. Its hosted upload and browser acceptance remain open because the same
portal source files also contain other pending portal work and must be released
as one reviewed package.

## Validation

- notification unit suite: 78 tests passed
- dashboard authentication/settings contracts: 22 tests passed
- dashboard JavaScript syntax checks passed
- `git diff --check` passed

