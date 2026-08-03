# ONB-001.B2 Functional Validation

> **Result:** Passed in controlled development
>
> **Date:** 2026-08-03
>
> **Stack:** `mot-dev-onboarding`, `eu-north-1`

## Scope

Validate the complete controlled portal flow with two confirmed Cognito users and
the existing `beta-01` WROOM identity. No Thing, certificate, policy, firmware or
telemetry-state record was changed.

## Preconditions

- `news@muehlberg.ch` was explicitly added to `mot-beta-admins`;
- `info@muehlberg.ch` remained outside every administrator group;
- the new claims, ownership and audit tables were empty;
- the existing ACTIVE B1 assignment protected `beta-01` from claim issuance;
- Thing `mot-esp32-f924f0-beta01`, its certificate association and 24 state records
  were inventoried before the assignment test.

## Evidence

| Test | Result |
|---|---|
| Normal user cannot see the administration panel | Passed |
| Admin user sees the administration panel after a new token is issued | Passed |
| Claim issuance for an existing ACTIVE B1 assignment fails closed | Passed; no claim/audit/ownership mutation |
| Exact ACTIVE assignment removal leaves Cognito, revoked history, Thing, certificate and telemetry intact | Passed |
| User with no assignment sees the portal claim form | Passed |
| Admin issues one claim for `beta-01` | Passed |
| User consumes claim through the portal | Passed |
| Existing user expands "Fahrzeug hinzufügen" and consumes a second vehicle claim | Passed with `beta-02`; existing `beta-01` assignment retained |
| Vehicle selector exposes both assignments after claim refresh | Passed |
| Existing telemetry appears again after atomic assignment | Passed |
| Claim record is `CONSUMED` with zero failed attempts | Passed |
| Canonical ownership is ACTIVE for the consuming Cognito subject | Passed |
| `UserVehicleAccess` is ACTIVE/OWNER with source `onb-001-b2-claim` | Passed |
| Audit contains `CLAIM_ISSUED` and `CLAIM_CONSUMED` with 90-day TTL | Passed |
| Lambda logs contain no proof, token, email or request body | Passed |

The issue and consume events were 38 seconds apart. The four claim-consumption
writes succeeded as one transaction; ownership, authorization and audit agree on
the same vehicle and consuming subject. The plaintext proof was handled only by the
two portal sessions and was not copied into this evidence.

## Effective state after validation

- `info@muehlberg.ch` owns and can select `beta-01` and `beta-02` after the
  additional-vehicle claim flow;
- `news@muehlberg.ch` retains its independent vehicle access and the controlled
  onboarding administrator role;
- the earlier REVOKED `news`/`beta-01` access item remains historical evidence;
- consumed claim records remain until DynamoDB TTL cleanup;
- two privacy-safe onboarding audit events remain for 90 days;
- device ingestion continues with the original Thing/certificate.

The local server access log exposed consumed OAuth callback query parameters during
the test. This does not invalidate PKCE or the completed flow, but it created
CLOUD-017. Future local tests use `tools/serve_dashboard.py`, which strips query
parameters from request logs; production hosting still requires verification.

## Remaining gates

- prove replay denial through an automated deployed-API regression without exposing
  a real proof in test output;
- decide whether the administrator role remains permanently assigned after beta;
- retain regression coverage for both empty-account and additional-vehicle claim UI;
- implement B3 replacement/transfer/recovery separately;
- retain the exact-assignment inventory gate before any future destructive test.

## Related documents

- [ONB-001.B work package](../project/sprints/ONB-001-B.md)
- [B2 deployment review](../administrator/aws/onboarding-change-set-review.md)
- [ONB-001.A validation](ONB-001-A-validation.md)
