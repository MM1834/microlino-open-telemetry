# Dashboard Authentication

## Scope

The dashboard uses a Cognito User Pool Authorization Code flow with PKCE. API
Gateway validates the JWT before requests reach Lambda.

## Module responsibilities

### `auth-manager.js`

Public facade:

- `login()`
- `logout()`
- `restoreSession()`
- `refresh()` — reserved contract; not implemented in Phase 2
- `getAccessToken()`
- `isAuthenticated()`
- `isConfigured()`
- `describe()`

It coordinates redirects, callback validation, token exchange and session
restoration. It contains no telemetry rendering or provider polling logic.

### `pkce.js`

Owns only:

- high-entropy verifier generation;
- SHA-256/S256 challenge generation;
- state generation;
- nonce generation.

It uses the browser Web Crypto API and therefore requires HTTPS or localhost.

### `token-store.js`

Owns persisted token response data and expiry metadata. The default storage is
`sessionStorage`, so closing the browser session removes the login state. It
contains no Cognito redirects and no API calls.

### `app.js`

Owns authentication controls and startup policy. For `aws-backend`, the data
provider starts only after `restoreSession()` reports a valid session. For
`legacy-mqtt`, authentication remains inactive.

### `aws-backend-provider.js`

Receives `getAccessToken` as a dependency and adds `Authorization: Bearer ...`
to API requests. It does not own Cognito state or token persistence.

## Browser flow

1. User explicitly selects **Anmelden**.
2. AuthManager stores verifier, state and nonce in `sessionStorage`.
3. Browser redirects to Cognito Hosted UI.
4. Cognito redirects to the exact configured dashboard URI with `code` and
   `state`.
5. AuthManager validates state and exchanges the code with the verifier.
6. Tokens and expiry metadata are stored for the browser session.
7. Callback parameters are removed from the visible URL.
8. The AWS provider starts and requests the Vehicle API with the access token.

## Security rules

- Authorization Code + PKCE only.
- S256 challenge method only.
- No implicit flow.
- No client secret in the browser.
- No manually copied tokens as a production mechanism.
- Tokens must never be written to application logs.
- Callback state is mandatory and single-use.
- Nonce is validated when present in the returned ID token.
- Configured issuer and client audience are checked when present in ID-token
  claims.
- Browser-side decoding is not signature validation.
- JWT signature and authorization validation remain API Gateway
  responsibilities.
- The browser receives no AWS IoT device credentials.

## Phase 2 boundary

Refresh-token rotation, roles and user-to-vehicle permissions remain outside
this sprint. An expired access token causes the local session to be cleared and
requires a new explicit login.
