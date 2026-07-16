# AWS-3.3 — Read-only vehicle API

AWS-3.3 updates the existing CloudFormation stack with:

```text
Dashboard / HTTP client
  -> API Gateway HTTP API
  -> read-only Lambda
  -> DynamoDB current-state table
```

## Routes

```text
GET /health
GET /api/vehicles
GET /api/vehicles/{vehicleId}/snapshot
```

Example snapshot:

```json
{
  "vehicleId": "pioneer",
  "updatedAt": 1784139636030,
  "online": true,
  "lastSeenUtc": 1784139635,
  "values": {
    "status/online": true,
    "system/last_seen_utc": 1784139635,
    "system/network_mode": "WiFi"
  },
  "metadata": {
    "system/last_seen_utc": {
      "receivedAt": 1784139636030,
      "valueType": "number",
      "payloadBytes": 10
    }
  }
}
```

The `values` shape is already compatible with the Dashboard's
`aws-backend` provider.

## Deploy

```bash
./tools/aws/deploy_aws_3_3.sh
```

For the first test, CORS defaults to `*`.

When the Dashboard origin is known:

```bash
MOT_API_ALLOWED_ORIGIN='https://dashboard.example.com' \
./tools/aws/deploy_aws_3_3.sh
```

## Test

```bash
./tools/aws/test_aws_3_3_api.sh pioneer
```

## Security warning

This beta API is deliberately unauthenticated so the cloud/data contract can
be tested before AWS-4 introduces Cognito and user-to-vehicle authorization.

Do not treat this API as an end-user production endpoint. Vehicle IDs are not
an authorization boundary.

## Multi-vehicle behavior

`GET /api/vehicles` currently scans the small beta state table and returns one
summary per vehicle. This is acceptable for the beta phase.

AWS-4 will replace this discovery mechanism with an authenticated
user-to-vehicle ownership table. Users will then receive only vehicles they
are authorized to access.
