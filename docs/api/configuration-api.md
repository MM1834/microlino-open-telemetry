# Configuration API

## GET `/api/config`

Returns the current configuration as JSON using schema version 1. This is an alias of the existing `/api/config/export` backup route.

## POST `/api/config`

Imports a JSON request body. Existing fields not present in the request remain unchanged.

Success:

```json
{"ok":true,"rebootRequired":true}
```

Invalid JSON or an unsupported future schema version returns HTTP 400.

## POST `/api/config/import`

Compatibility alias for `POST /api/config`.

## GET `/api/readiness`

Returns shared configuration and runtime readiness:

```json
{
  "schemaVersion": 1,
  "configured": true,
  "ready": false,
  "checks": {
    "onboarding": {"required": true, "configured": true},
    "network": {"required": true, "configured": true, "online": true},
    "can": {"required": true, "configured": true, "online": false},
    "gps": {"required": false, "detected": true, "fix": false, "state": "GPS_DETECTED"},
    "mqtt": {"required": true, "enabled": true, "configured": true, "online": true},
    "aws": {"required": false, "enabled": false, "configured": false},
    "abrp": {"required": false, "enabled": false, "configured": false}
  }
}
```

## Security note

B.8 deliberately keeps the local endpoints unauthenticated. Do not expose the device WebUI directly to the public internet. Remote portal provisioning requires authentication and transport security in a later sprint.
