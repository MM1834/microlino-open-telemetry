# ADM-GRANT-001 — Active administrator grant inventory

> **Status:** Complete — repository, hosted portal and AWS backend accepted
>
> **Started:** 2026-09-09
>
> **Completed:** 2026-09-09

## Objective

Show authorized administrators the currently valid WebFlash and local-password
recovery permissions on the existing Administration page without exposing the
grant table to browsers or standard users.

## Scope and behaviour

- add JWT-protected `GET /api/admin/grants` to the onboarding boundary;
- require the existing `mot-beta-admins` Cognito group in the Lambda;
- scan at most 200 encrypted grant records and fail closed rather than return a
  partial inventory;
- include only `ACTIVE`, non-expired password-recovery grants;
- include WebFlash grants only when target, version and SHA-256 still match an
  active release;
- resolve portal email addresses server-side through Cognito and return no grant
  creator identity or audit details;
- render separate responsive WebFlash and password-recovery tables;
- refresh on page entry, on demand and after grant/revoke operations.

## Security and compatibility boundary

The browser never receives DynamoDB access. API Gateway JWT authorization and a
second Lambda-side administrator group check are both required. DynamoDB TTL is
not treated as an authorization clock: `expiresAt` is evaluated for every list
request. Existing grant, revoke, access, download and recovery rules remain
unchanged and authoritative.

## Acceptance

- [x] Standard users receive `403` from the list handler.
- [x] Expired, revoked and release-mismatched grants are omitted.
- [x] A truncated scan fails closed instead of displaying a partial inventory.
- [x] Portal rows are constructed with `textContent`, not HTML injection.
- [x] German, English, French and Italian labels are present.
- [x] All 27 onboarding tests and 32 focused portal/i18n tests pass.
- [x] CloudFormation template validation passes in `eu-north-1`.
- [x] CloudFormation change set reviewed and deployed without replacement.
- [x] Live administrator inventory verified with active and empty states.
- [x] Desktop and smartphone layouts accepted by the maintainer.
- [x] Repeated practical WebFlash grant/revoke tests passed.
- [x] Repeated practical password-recovery grant/revoke tests passed.

## Deployment evidence

The immutable Lambda package has SHA-256
`95122879896d82d6c3f57e8f5a438ae47e67eb01d80ac4cb7904750b09d3c85d`
and is stored as
`onboarding/adm-grant-001/onboarding-lambda-95122879896d82d6c3f57e8f5a438ae47e67eb01d80ac4cb7904750b09d3c85d.zip`.
Reviewed Change Set `adm-grant-001-20260909` added only `ActiveGrantsRoute`
and modified the existing Lambda, integration and IAM role with
`Replacement: False`. Stack `mot-dev-onboarding` returned to
`UPDATE_COMPLETE`.

Effective API read-back reports JWT authorization for
`GET /api/admin/grants`. Anonymous access returns `401`, direct non-admin handler
validation returns `403`, and the administrator smoke test returns `200` with
one active WebFlash record and an empty password-recovery list, without exposing
email addresses in the validation output. The effective IAM policy contains only
the existing `AdminGetUser` plus the new `ListUsers` action for Cognito user
resolution.

The maintainer subsequently completed diverse productive portal tests for both
WebFlash and password-recovery permissions, including their inventory updates,
and granted final acceptance on 2026-09-09.
