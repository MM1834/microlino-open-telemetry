# Live WebSocket API

> **Status:** Implemented in CloudFormation and dashboard code; deployment not revalidated
>
> **Audience:** Portal developer, backend developer and administrator
>
> **Sources of truth:** `cloud/aws/foundation/template.yaml` and
> `dashboard/js/live/websocket-client.js`

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

JWT-authorized. Stores connection ID, user subject, connection time and TTL in the
live-connections table.

### `$disconnect`

Deletes the connection record.

### `subscribe`

Request:

```json
{"action":"subscribe","vehicleId":"pioneer"}
```

Updates the connection with the requested `vehicleId` and returns a subscribed
message.

Current security limitation: the handler does not verify that the authenticated
`userSub` may access the requested vehicle.

### `ping`

Maintains heartbeat/TTL behaviour and returns a pong-style response.

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

The portal discards messages whose vehicle ID does not match its active selection.
Client-side filtering is not authorization.

## Required onboarding change

The backend must authorize `subscribe` against the same access model used by the
Vehicle REST API. Authorization must remain valid across reconnects and vehicle
switches, with negative tests for guessed vehicle IDs.

## Related documents

- [Vehicle REST API](vehicle-api.md)
- [Authentication flow](../architecture/authentication-flow.md)
- [AWS architecture](../architecture/aws-iot.md)
