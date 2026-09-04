# DAY-SUM-001 — Daily Journey and Charging Summary

> **Status:** Closed — scheduled delivery and report contents accepted
>
> **Date:** 2026-09-03

## Objective

Offer an optional per-user/per-vehicle email containing daily totals for completed
journeys and charging sessions. Reuse the accepted individual-summary evidence,
the confirmed email channel and existing idempotent event store without deriving
totals from chart History.

## Time and attribution contract

- The reporting day is the preceding calendar day in `Europe/Zurich`.
- AWS Scheduler evaluates the report hourly at minute 05 from 00:05 through
  08:05 in that timezone, including CET/CEST transition days.
- A journey or charging session belongs wholly to the local date on which it
  ends; sessions are not split artificially at midnight.
- While a session is active, delivery waits for the next hourly evaluation. At
  08:05 it proceeds and states that the active session is excluded and will
  belong to its completion day.
- A day without a completed journey or charging session produces no email.

## Preference and delivery boundary

`dailySummaryEmailEnabled` is additive, defaults to false and requires the
existing email channel. It is independent from both individual journey and
individual charging-summary switches. Daily summaries are email-only; SMS is
explicitly excluded.

The event identifier is deterministic for user, vehicle and local report date.
Conditional creation permits at most one daily email under scheduler retries.
The existing retained individual completion events carry the aggregate fields
required by the daily report. A daily-only subscriber records those completion
events without receiving the corresponding individual email.

## Report contents

- journey count, distance and duration;
- energy drawn, regeneration, net energy and aggregate net kWh/100 km;
- charging-session count, duration, estimated charged energy and SOC increase;
- bounded ongoing-session note when the 08:05 deadline is reached.

The report remains passive telemetry and is neither billing nor precision
measurement.

## Acceptance

- [x] Winter CET and summer CEST UTC boundaries are deterministic.
- [x] The spring transition day is represented as a 23-hour calendar day.
- [x] Journey and charging totals include only events ending inside the day.
- [x] Empty days are suppressed.
- [x] Preference defaults off and rejects activation without email.
- [x] Scheduler uses `Europe/Zurich` and invokes no SMS path.
- [x] Deploy through a reviewed no-data-resource-replacement CloudFormation Change Set.
- [x] Enable controlled users and accept delivered daily reports with correct totals.
- [x] Confirm bounded retry/deferral behaviour through deterministic tests and the
  successful overnight Scheduler delivery path.

## Deployment evidence

On 2026-09-03 AWS validated the repository template and Change Set
`day-sum-001-20260903` was reviewed before execution. It added only the dedicated
Scheduler role and `Europe/Zurich` schedule, updated the existing notification
role in place with exact table scans, and updated the three shared Lambda code
packages in place. No table or Lambda function was replaced. Stack
`mot-dev-notifications` reached `UPDATE_COMPLETE`; the scheduler resources report
`CREATE_COMPLETE`, the notification and preference Lambdas report `Active` and
`Successful`, and the schedule read-back is enabled with
`cron(5 0-8 * * ? *)`, timezone `Europe/Zurich` and the expected
`{"type":"daily_summary"}` payload. The public preference route remains
JWT-protected and returned 401 without credentials.

A direct scheduled-event invocation was deliberately not used as a smoke test,
because an opted-in live record could make that probe send a real telemetry
email. The hosted portal subsequently passed. On 2026-09-04 the overnight
Scheduler prepared and delivered the enabled users' daily emails successfully;
the maintainer verified the reported journey and charging data as correct. This
closes DAY-SUM-001. Future per-user timezone support remains a separate optional
enhancement and does not reopen this sprint.
