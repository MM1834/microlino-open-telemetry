# NTF-FIX-001 — Reconcile Email Confirmation State

> **Status:** Completed — deployed and hosted portal acceptance passed
>
> **Opened:** 2026-08-15

## Problem

SNS can confirm an email subscription while the notification preference record
continues to contain `emailConfirmed=false`. The portal then shows
`Bestätigung ausstehend` even though SNS already accepts delivery for the
recipient filter.

Read-only production diagnosis confirmed this split state for the controlled
`xrpioneer2` pilot: SNS reported `PendingConfirmation=false`, while the DynamoDB
preference flag remained false. Delivery eligibility is controlled by SNS and was
therefore active; the portal status was stale.

## Fix

- treat SNS subscription attributes as the authoritative confirmation state;
- reconcile `emailConfirmed` during authenticated preference reads;
- update DynamoDB conditionally against the same subscription ARN so a concurrent
  email-address change cannot be overwritten;
- retain `false` for genuinely pending subscriptions;
- grant the preference Lambda only `sns:GetSubscriptionAttributes` and the
  conditional DynamoDB update needed for reconciliation;
- cover confirmed and pending states with focused tests.

## Acceptance gates

- [x] Controlled production discrepancy diagnosed read-only.
- [x] Confirmed SNS subscription changes the returned and stored flag to true.
- [x] Pending SNS subscription remains unconfirmed.
- [x] Notification stack Change Set reviewed and deployed without replacement.
- [x] Deployed API reconciliation returns and stores `emailConfirmed=true` for
  the controlled `xrpioneer2` record.
- [x] Hosted portal shows `E-Mail-Adresse bestätigt` for `xrpioneer2` after reload.
- [x] Existing charging/SOC and journey-notification tests remain green.

## Boundary

The fix does not resend confirmation mail, recreate subscriptions or alter login
persistence. AUTH-PERSIST-001 remains closed; the reported pilot-browser login
behaviour waits for browser/private-mode details from the user.

## Deployment evidence

Change Set `ntf-fix-001-scoped-20260815` modified only the existing
`PreferenceApiFunction`, its least-privilege IAM role and the in-place API
integration, all with `Replacement: False`. The notification/journey runtime
artifact was explicitly preserved. Stack `mot-dev-notifications` reached
`UPDATE_COMPLETE`; the Lambda reported `Active` and `Successful`. A direct
authorized GET for the controlled record returned HTTP 200 without a function
error and reconciled DynamoDB from false to true while SNS continued to report
`PendingConfirmation=false`.

On 2026-08-20 the maintainer accepted the confirmed-address behaviour in the
hosted portal after reload. The portal reported the address as confirmed and hid
the subscription-confirmation guidance until the confirmed address was edited.
This closes the final NTF-FIX-001 acceptance gate.
