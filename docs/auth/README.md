# Authentication Documentation

> **Status:** Current index; child documents are historical increment notes
>
> **Audience:** Developer and administrator

## Current sources

- [Authentication architecture](../architecture/authentication.md)
- [Authentication flow](../architecture/authentication-flow.md)
- [Vehicle REST API](../api/vehicle-api.md)
- [Live WebSocket API](../api/live-websocket-api.md)
- [User-to-vehicle assignment operations](admin/user-vehicle-assignments.md)

## Current repository state

CloudFormation contains the Cognito User Pool, public dashboard app client,
managed-login domain and JWT authorizers. The dashboard implements Authorization
Code with PKCE. Public self-registration is disabled by default.

The documents below `admin/`, `developer/` and `end-user/` were written as Cognito
was added incrementally. Statements such as “the app client/domain/login/route
protection is added later” are historical and no longer describe current code.

## Missing authorization boundary

Cognito authenticates a person. It does not assign that person to a vehicle.
User-to-vehicle enforcement is deployed and validated. ONB-001.B1 provides a local
guarded administrator invitation/assignment workflow. Portal claim proofs, device
claiming, transfer and recovery remain planned.
