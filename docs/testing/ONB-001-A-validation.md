# ONB-001.A Authorization Validation Plan

> **Status:** Development deployment complete; authenticated multi-user checks pending
>
> **Audience:** Backend developer, security reviewer and beta tester coordinator

## Purpose

Prove that an authenticated portal user can access only explicitly assigned
vehicles through both REST and WebSocket APIs. Device claiming is not part of this
slice; assignments are administered directly for the controlled beta.

## Required identities and fixtures

- user A with Cognito subject `user-a`;
- user B with Cognito subject `user-b`;
- user C with no assignment;
- vehicle A assigned ACTIVE only to user A;
- vehicle B assigned ACTIVE only to user B;
- one REVOKED assignment fixture;
- telemetry state for both vehicles containing distinguishable synthetic values.

Use real Cognito `sub` values only in the controlled test record, never in public
Git. No access token, refresh token, email, claim proof or device credential belongs
in screenshots, terminal transcripts or committed fixtures.

## Pre-deploy gates

- [x] Template parses and AWS CloudFormation validation succeeds.
- [x] Access table uses `userSub`/`vehicleId` keys and encryption.
- [x] REST role has no state-table `Scan` permission.
- [x] REST list derives assignments from validated JWT `sub`.
- [x] Snapshot checks ACTIVE assignment before reading vehicle state.
- [x] Missing subject, missing assignment and REVOKED assignment fail closed locally.
- [x] WebSocket authorizer passes validated `sub` and numeric token expiry.
- [x] Connection expiry never exceeds token expiry.
- [x] Subscribe validates ACTIVE assignment and prevents vehicle switching bypass.
- [x] Ping cannot extend authorization.
- [x] Fan-out removes/skips expired and no-longer-authorized connections.
- [x] CORS is the exact approved development origin `http://localhost:8080`.
- [x] Callback/logout URLs match the local development portal.
- [x] Inspect deployed authorizer IAM and post-deploy logs for token-term exposure.
- [x] Both Change Sets were reviewed before execution.

## Automated negative tests

| Case | Expected result |
|---|---|
| Protected REST event without `sub` | 401; no DynamoDB state read |
| User A lists vehicles | Vehicle A only |
| User C lists vehicles | Empty list, not all vehicles |
| User A requests vehicle B snapshot | 404 or equivalent non-enumerating denial |
| REVOKED assignment requests snapshot | Denied |
| WebSocket connection lacks subject/expiry | Rejected or immediately unusable |
| User A subscribes to vehicle B | Denied; prior authorized subscription not widened |
| Ping after token expiry | Denied and connection record removed |
| Expired connection during fan-out | No telemetry; record removed |
| Revoked assignment during fan-out | No telemetry; record removed or disconnected |

## Post-deploy maintainer checks

1. Record deployed stack, region, change-set ID and exact Git commit.
2. Create only the minimum controlled assignments.
3. Confirm user A REST list/snapshot and live telemetry for vehicle A.
4. Repeat for user B and vehicle B.
5. Perform every cross-user guessed-ID negative test in both APIs.
6. Confirm user C sees a valid empty/onboarding-required portal state.
7. Revoke one assignment and confirm bounded revocation propagation.
8. Allow a token to expire and confirm live access ends without a successful ping.
9. Inspect API/Lambda logs for tokens, emails and unnecessary identifiers.
10. Confirm device-to-cloud ingestion remains operational for both vehicles.

## Beta-user acceptance flow

After security tests pass, a tester validates only their own account:

- login/logout and session-expiry behaviour;
- exactly one expected vehicle shown;
- snapshot values and live updates correspond to that physical vehicle;
- no generic error when no vehicle is assigned;
- support contact and safe-data instructions are reachable.

The tester is never asked to inspect developer tools for tokens or to attempt access
to another real tester's vehicle. Cross-user security tests use controlled accounts
and synthetic fixtures under maintainer supervision.

## Evidence result

Record each result as Pass, Fail or Not run. “Implemented in CloudFormation” is not
deployment or runtime evidence. Any cross-user data disclosure blocks beta release.

### Development deployment 2026-08-02

| Check | Result | Evidence |
|---|---|---|
| Local runtime and structural suites | Pass | 16 tests |
| AWS template validation | Pass | `eu-north-1` |
| First Change Set | Fail safely | AWS rejected an attempted GSI projection change and rolled the stack back |
| Corrected Change Set | Pass | `onb-001-a-20260802-2`; no deletes or replacements |
| Stack completion | Pass | `mot-aws-3-1` reached `UPDATE_COMPLETE` |
| Exact development CORS origin | Pass | `http://localhost:8080` |
| Initial controlled assignments | Pass | `beta-01` and `pioneer`, ACTIVE/OWNER, sole existing confirmed subject |
| Public health endpoint | Pass | HTTP 200 |
| Protected endpoint without bearer token | Pass | HTTP 401 |
| Authorizer least privilege | Pass | Basic Lambda logging policy only; no inline policies |
| Post-deploy `access_token` log-term check | Pass | Zero hits in authorizer and live-handler logs |
| Cognito managed-login endpoint | Failed, corrected | Initial output used `.amazonaws.com`; regression fix uses `.amazoncognito.com` |
| Authenticated two-user/two-vehicle isolation | Not run | Second controlled Cognito identity not yet available |

No email, token, password, certificate or private key is recorded in this evidence.
