# Vehicle REST API

> **Status:** Implemented in CloudFormation; deployment not revalidated
>
> **Audience:** Portal developer, backend developer and administrator
>
> **Source of truth:** `cloud/aws/foundation/template.yaml`

## Trust boundary

API Gateway validates a Cognito JWT before vehicle requests reach the Lambda.
ONB-001.A additionally derives the Cognito `sub` in Lambda and checks server-side
`UserVehicleAccess` assignments. This implementation is local and must not be
treated as deployed until the change set and negative tests pass.

## Endpoints

### `GET /health`

Public. Returns service, project, environment and server UTC information. It must
not return vehicle or user data.

### `GET /api/vehicles`

Requires a JWT accepted by `VehicleApiJwtAuthorizer`. Queries ACTIVE assignments
for the authenticated subject, then returns summaries only for those vehicles. It
does not scan the vehicle-state table.

### `GET /api/vehicles/{vehicleId}/snapshot`

Requires a JWT and ACTIVE assignment. The handler checks access before querying the
DynamoDB state partition. Unknown, inactive and unassigned vehicle IDs share the
same non-enumerating 404 response.

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

## Deployment gate

Run cross-user list/snapshot tests after deployment. The client-selected `vehicleId`
is never sufficient evidence of authorization.

## Related documents

- [Authentication architecture](../architecture/authentication.md)
- [Live WebSocket API](live-websocket-api.md)
- [AWS architecture](../architecture/aws-iot.md)
- [Onboarding authorization](../architecture/onboarding-authorization.md)
- [Historical AWS-3.3 record](../aws/AWS-3-3-vehicle-api.md)
