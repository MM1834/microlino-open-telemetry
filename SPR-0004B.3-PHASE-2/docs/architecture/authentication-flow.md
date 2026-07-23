# Authentication Flow

```text
Browser Dashboard
    |
    | 1. Generate verifier, S256 challenge, state, nonce
    | 2. GET /oauth2/authorize
    v
Cognito Hosted UI
    |
    | 3. User authentication
    | 4. Redirect with authorization code + state
    v
Browser Dashboard
    |
    | 5. Validate state
    | 6. POST /oauth2/token with code + verifier
    v
Cognito Token Endpoint
    |
    | 7. Access token (+ ID/refresh token when configured)
    v
Browser sessionStorage
    |
    | 8. Authorization: Bearer <access token>
    v
API Gateway HTTP API JWT Authorizer
    |
    | 9. Validate signature, issuer, audience and expiry
    v
Lambda Vehicle API
```

## Failure paths

- Missing or mismatched state: reject callback, clear transaction and tokens.
- Hosted UI error: show the returned description and do not start the provider.
- Failed token exchange: clear local authentication state.
- Expired local session: clear it and require login.
- API HTTP 401: report reauthentication requirement; Phase 2 does not refresh
  automatically.

## Trust boundary

Cognito authenticates the user. API Gateway is the authorization enforcement
point. The browser is not trusted to validate JWT signatures or decide vehicle
permissions.
