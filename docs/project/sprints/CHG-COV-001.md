# CHG-COV-001 — Transparente Ladeenergie und Datenabdeckung

> **Status:** Backend deployed; natural charging-email acceptance open
>
> **Started:** 2026-09-13

## Objective

Make charging-summary energy quality explicit when telemetry gaps prevent full
power integration. Preserve the measured pack-side value, add an SOC-based
estimate from the declared vehicle capacity, and show power-data coverage in the
email and persisted event.

## Evidence and motivation

A read-only review of 48 stored charging summaries across five vehicles found 36
events with SOC, energy and declared capacity available for comparison. Twenty-eight
were within 85–120 percent of the SOC-based expectation, five were below 70
percent and none exceeded 130 percent. The severe undercounts were session-specific:
the same vehicles produced correct results when telemetry remained continuous.

For `ml-pilot-023`, six summaries totaled 120 SOC percentage points and 12.64 kWh,
almost exactly the 12.60 kWh expected from its declared 10.5 kWh profile. One
individual 2026-09-12 session nevertheless reported only 0.40 kWh for a 10-point
SOC rise, versus an SOC estimate of 1.05 kWh. The deployed email used the same
state and therefore carried that incomplete measured value.

The current integrator deliberately ignores power intervals longer than 30 seconds.
This avoids inventing energy but causes undercounting during WiFi, mobile-network,
adapter-power or publication gaps. Offline cache Backfill cannot repair the email:
it retains only SOC and Speed and writes History only.

## Accepted behaviour

- Continue to integrate only directly observed pack-side charging power.
- Track integrated power duration, largest power gap and sample count per plugged
  charging session.
- Calculate coverage as integrated power duration divided by the qualified session
  duration.
- Preserve measured energy as the primary evidence value.
- Label measured energy as a minimum when coverage is below 95 percent.
- Always show the power-data coverage when available.
- Read the declared `batteryCapacityKwh` profile and show
  `SOC delta × capacity / 100` as a separate estimate when both inputs exist.
- Never substitute the estimate for measured energy.
- Persist coverage, largest gap, sample count, capacity and SOC estimate in new
  additive charging-event attributes.
- Keep German as the missing-language fallback and provide equivalent English,
  French and Italian wording.
- Do not query the diagnostic/debug history path.

## Repository implementation

- `charging_summary_state.py` now tracks `covered_power_ms`,
  `largest_power_gap_ms` and `power_sample_count` without integrating gaps longer
  than the existing 30-second safety bound.
- `handler.py` calculates coverage and the SOC-based estimate, stores the additive
  evidence fields and passes both values to the email renderer.
- The Notification Lambda receives read-only access to the existing vehicle-profile
  table; no table replacement or migration is required.
- `email_templates.py` renders both energy values and coverage in all four supported
  languages. Below 95 percent, measured energy is explicitly a minimum and the
  telemetry interruption is named.

## Validation

- 106 notification unit and contract tests pass.
- Python syntax validation passes for the changed notification modules.
- CloudFormation validation passes.
- The exact notification ZIP has SHA-256
  `fb2035dd7c11453615582940f1e11fd4c12e2f4436307873541eda376a84340a`.
- The deployed Lambda reports `Active` / `Successful`, exposes the vehicle-profile
  table name and has an exact read-only `dynamodb:GetItem` policy for that table.
- A send-free invalid-topic invocation returned HTTP 200 without a function error;
  the checked Lambda error log was empty.
- Productive acceptance requires one continuous and one deliberately interrupted
  charging session.

## Deployment boundary

The first two full-stack Change Sets were deliberately deleted without execution:
existing stack drift would also have modified Scheduler, EventBridge and IoT
resources and made one Lambda permission conditionally replaceable. The accepted
2026-09-13 deployment therefore followed the established isolated-notification
path: only the existing Notification Lambda code/configuration and a separate
least-privilege profile-table read policy changed. The parent stack remained
`UPDATE_COMPLETE`; no email was emitted during technical validation. Inspect the
next natural charging email for both energy values and coverage, then perform one
deliberately interrupted-session acceptance test.
