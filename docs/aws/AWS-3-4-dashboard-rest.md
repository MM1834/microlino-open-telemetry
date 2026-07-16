# AWS-3.4 — Dashboard over REST

The Dashboard now uses the AWS Vehicle API by default.

```text
GET /api/vehicles
GET /api/vehicles/{vehicleId}/snapshot
```

Snapshots are polled every five seconds. If multiple vehicles exist, a selector
appears in the header. The Legacy MQTT provider remains available by changing
`dataSource.type` back to `legacy-mqtt`.

The API is still unauthenticated; AWS-4 adds Cognito and per-user vehicle
authorization.
