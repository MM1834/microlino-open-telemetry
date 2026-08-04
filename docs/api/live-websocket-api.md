# Live WebSocket API

> **Status:** Deployed and multi-user validated in the controlled AWS development stack
>
> **Audience:** Portal developer, backend developer and administrator
>
> **Sources of truth:** `cloud/aws/foundation/template.yaml` and
> `build/dashboard/current/js/live/websocket-client.js`

## Connection authentication

The browser connects to the WebSocket URL with an access token in the
`access_token` query parameter. A custom Lambda authorizer:

- downloads and caches Cognito JWKS;
- verifies RS256 signature;
- checks issuer;
- requires `token_use=access`;
- checks Cognito client ID, expiry and optional not-before;
- requires a subject (`sub`).

The token in the URL is sensitive. It must not be logged, copied into diagnostics
or retained by intermediaries. A future review should consider alternatives and
confirm API Gateway access-log redaction.

## Routes

### `$connect`

JWT-authorized. Stores connection ID, user subject, connection time and token expiry
in the live-connections table. Connection TTL never exceeds access-token expiry.

### `$disconnect`

Deletes the connection record.

### `subscribe`

Request:

```json
{"action":"subscribe","vehicleId":"pioneer"}
```

Checks ACTIVE `UserVehicleAccess` for the stored subject before updating the
connection with the requested `vehicleId`. Missing/inactive assignments receive a
non-enumerating denial.

### `ping`

Returns a pong only while the connection token remains valid. Ping does not extend
the stored authorization lifetime.

### `$default`

Handles unsupported or malformed actions through the shared handler.

## Live telemetry

The state-ingestion Lambda queries connections by `vehicleId` and sends messages:

```json
{
  "type": "telemetry",
  "vehicleId": "pioneer",
  "topic": "mot/pioneer/status/online",
  "topicSuffix": "status/online",
  "value": true,
  "valueType": "boolean",
  "receivedAt": 0
}
```

Before sending, ingestion rechecks token expiry and ACTIVE assignment. Expired or
revoked connection records are removed. Portal filtering remains UX only and is not
part of authorization.

## Deployment gate

ONB-001.A passed controlled two-user isolation, guessed vehicle, expiry and live
revoke/restore tests. Production configuration must repeat the security gate.

## Related documents

- [Vehicle REST API](vehicle-api.md)
- [Authentication flow](../architecture/authentication-flow.md)
- [AWS architecture](../architecture/aws-iot.md)
- [Onboarding authorization](../architecture/onboarding-authorization.md)
