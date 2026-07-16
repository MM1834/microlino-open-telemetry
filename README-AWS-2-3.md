# AWS-2.3 — Telemetry contract and dashboard provider

This package introduces a data-source boundary without changing the visible dashboard behavior.

## Current mode

```javascript
dataSource: {
  type: "legacy-mqtt"
}
```

The current MQTT/WebSocket dashboard continues to work.

## Future mode

```javascript
dataSource: {
  type: "aws-backend"
}
```

The included AWS backend provider defines the expected snapshot and WebSocket contracts, but requires the later authenticated backend/user-management sprint.

## Files

```text
dashboard/js/providers/provider-registry.js
dashboard/js/providers/legacy-mqtt-provider.js
dashboard/js/providers/aws-backend-provider.js
dashboard/js/app.js
dashboard/config.js
dashboard/config.example.js
dashboard/index.html
docs/aws/AWS-2-3-telemetry-contract.md
docs/aws/AWS-2-3-dashboard-data-provider.md
```

## Test

With the default `legacy-mqtt` mode:

1. Load the dashboard.
2. Confirm the same persistent MQTT client ID.
3. Confirm live topics still update all widgets.
4. Confirm IP, network mode and OBD2 last-seen remain functional.

Do not switch to `aws-backend` until an authenticated API/WebSocket endpoint exists.
