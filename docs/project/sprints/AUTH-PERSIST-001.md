# AUTH-PERSIST-001 — Optional Persistent Portal Login

**Status:** Completed — hosted desktop and smartphone acceptance passed

**Opened:** 2026-08-14

**Completed:** 2026-08-14

## Objective

Allow a user to opt into a bounded login on a trusted smartphone so closing all
tabs or the browser does not require credentials again while the Cognito refresh
session remains valid. Preserve the existing browser-session-only behaviour for
every user who does not explicitly opt in.

## Scope

- add an unchecked `Angemeldet bleiben` control to the existing Cognito PKCE login;
- keep PKCE verifier, state and nonce transaction data in `sessionStorage`;
- store only the refresh token and its local expiry metadata persistently;
- renew the one-hour access token through Cognito's public-client refresh grant;
- cap local persistence at the deployed 30-day Cognito refresh-token lifetime;
- clear session and persistent storage on explicit logout or permanent refresh
  rejection;
- retain the refresh record after temporary network or throttling failures;
- preserve existing users' session-only login as the default.

## Security boundary

This is a controlled beta convenience feature for a trusted personal device. It
does not create a passwordless account, extend Cognito's server-side token
lifetime or place AWS/device credentials in the browser. Browser `localStorage`
cannot provide `HttpOnly` protection, so a later public-production design should
re-evaluate a backend-for-frontend cookie session. Private browsing and browser
storage eviction are explicitly not guaranteed persistence mechanisms.

## Acceptance gates

- [x] Default unchecked login continues using `sessionStorage` only.
- [x] Opted-in callback stores no access or ID token persistently.
- [x] A persisted refresh record obtains a new access token after browser restart.
- [x] Refresh requests are deduplicated while one is in flight.
- [x] Explicit logout clears both storage classes.
- [x] Permanent Cognito refresh rejection clears the persistent record.
- [x] Temporary network and throttling failures retain the persistent record.
- [x] Static contract tests and JavaScript syntax checks pass.
- [x] Hosted smartphone close-and-reopen acceptance passes.
- [x] Hosted desktop, default-login regression and explicit logout acceptance pass.

## Completion rule

Close only after the updated static portal has been uploaded and both the opted-in
and default session paths have passed on the hosted dashboard. No Cognito stack
change is required: the deployed app client already has 60-minute access/ID tokens,
a 30-day refresh token, authorization-code flow with PKCE and token revocation.

The maintainer confirmed the hosted desktop and smartphone paths on 2026-08-14.
The opt-in persisted across browser close/reopen, while the unchecked default and
explicit logout retained the expected session boundary. AUTH-PERSIST-001 is
therefore closed without a Cognito infrastructure change.
