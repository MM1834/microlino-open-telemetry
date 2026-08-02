# Portal Onboarding Authorization Foundation

> **Status:** Implemented locally in ONB-001.A; not deployed or cloud-validated
>
> **Audience:** Portal, backend and security developer

## Identity boundary

| Identifier | Authority | Purpose |
|---|---|---|
| Cognito `sub` | Cognito access token | Stable portal-user identity |
| `vehicleId` | MOT application/backend | Authorization resource and telemetry namespace |
| `deviceId` | Physical MOT unit | Inventory/support identity; later onboarding input |
| Thing name/certificate | AWS IoT | Device-to-cloud identity, never portal ownership |

Authentication alone grants no vehicle access. Assignment is stored server-side in
`UserVehicleAccess` using partition key `userSub` and sort key `vehicleId`. Only an
item with `status=ACTIVE` authorizes access.

## REST enforcement

- `/health` remains public and contains no user/vehicle data.
- `/api/vehicles` reads ACTIVE assignments for the validated JWT subject and loads
  only those vehicle summaries; it no longer scans the state table.
- `/api/vehicles/{vehicleId}/snapshot` verifies ACTIVE access before reading state.
- missing subject returns 401; missing/inactive assignment returns the same 404 as
  an unknown vehicle to reduce identifier enumeration.

## WebSocket enforcement

The request authorizer validates the access token and passes only subject and token
expiry to the handler. The connection record expires at the token expiry; ping does
not extend it. Every subscribe/switch checks ACTIVE assignment.

Telemetry fan-out rechecks token expiry and current assignment. Expired or revoked
connections are deleted before data is sent. This favours correctness over read
cost for the small beta fleet; caching/scaling is deferred until measurements exist.

## Controlled beta administration

ONB-001.A intentionally provides the authorization substrate, not public claiming.
Maintainers will create ACTIVE assignments through a reviewed administrative
procedure after deployment. ONB-001.B adds invitation/claim lifecycle and portal UI.

## Deployment boundary

The template has no wildcard CORS or localhost callback/logout defaults. Deployments
must supply exact environment values. Local tests compile all inline Lambda code and
exercise cross-user REST/WebSocket denial, expiry and revocation. AWS deployment and
end-to-end Cognito/API validation remain explicit later gates.

## Related documents

- [ONB-001.A validation plan](../testing/ONB-001-A-validation.md)
- [Cloud risk register](../security/cloud-risk-register.md)
- [Vehicle API](../api/vehicle-api.md)
- [Live WebSocket API](../api/live-websocket-api.md)
