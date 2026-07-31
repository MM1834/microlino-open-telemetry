# Protected API Routes

> **Status:** Implemented in template; deployed state not revalidated
>
> **Audience:** Administrator and security reviewer

## Current boundary

The hosted HTTP API leaves `GET /health` public. These routes require a Cognito JWT:

- `GET /api/vehicles`
- `GET /api/vehicles/{vehicleId}/snapshot`

The WebSocket `$connect` route uses a custom Cognito JWT authorizer. Other
WebSocket messages are associated with the authenticated connection.

JWT protection authenticates users but does not enforce vehicle ownership. Before
multi-user beta access, validate that both REST requests and WebSocket subscriptions
reject an authenticated user who is not assigned to the requested vehicle.

## Verification boundary

Current route configuration is documented in:

- [Vehicle REST API](../../api/vehicle-api.md)
- [Live WebSocket API](../../api/live-websocket-api.md)
- `cloud/aws/foundation/template.yaml`

Operational AWS verification has not been performed during DOC-001.
