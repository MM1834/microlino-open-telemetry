# Vehicle REST API

> **Status:** Implemented in CloudFormation; deployment not revalidated
>
> **Audience:** Portal developer, backend developer and administrator
>
> **Source of truth:** `cloud/aws/foundation/template.yaml`

## Trust boundary

API Gateway validates a Cognito JWT before vehicle requests reach the Lambda. This
is authentication only. The Lambda does not currently restrict results by Cognito
`sub`, group or a user-to-vehicle access record.

Do not use the current API for multiple mutually untrusted beta users.

## Endpoints

### `GET /health`

Public. Returns service, project, environment and server UTC information. It must
not return vehicle or user data.

### `GET /api/vehicles`

Requires a JWT accepted by `VehicleApiJwtAuthorizer`. Scans the vehicle-state table
for distinct vehicle IDs and returns their online/last-seen summary.

Current security limitation: every authenticated user receives every discovered
vehicle.

### `GET /api/vehicles/{vehicleId}/snapshot`

Requires a JWT. Queries the DynamoDB partition matching `vehicleId` and returns
current values plus per-topic metadata.

Current security limitation: knowledge of a vehicle ID plus any accepted user token
is sufficient; ownership is not checked.

## Authentication

The HTTP API JWT authorizer uses:

- issuer: the stack's Cognito User Pool;
- audience: the dashboard User Pool Client ID;
- identity source: `Authorization` header.

The dashboard sends:

```http
Authorization: Bearer <access-token>
```

## Response behaviour

- JSON responses use `cache-control: no-store`;
- missing snapshots return HTTP 404;
- a missing vehicle path value returns HTTP 400;
- unknown routes return HTTP 404;
- API Gateway handles invalid/missing JWTs before Lambda.

## Required onboarding change

ONB-001 must define a server-side access model and apply it to both list and
snapshot operations. The client-selected `vehicleId` is never sufficient evidence
of authorization.

## Related documents

- [Authentication architecture](../architecture/authentication.md)
- [Live WebSocket API](live-websocket-api.md)
- [AWS architecture](../architecture/aws-iot.md)
- [Historical AWS-3.3 record](../aws/AWS-3-3-vehicle-api.md)
