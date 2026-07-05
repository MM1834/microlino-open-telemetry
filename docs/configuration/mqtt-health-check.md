# MQTT Health Check

MQTT status endpoints are used for diagnostics.

LilyGO:

```text
GET /api/lilygo/mqtt
GET /api/lilygo/mqtt/debug
```

Useful fields:

- enabled
- connected
- transport
- wantedTransport
- host
- port
- state
- stateText
- connectAttempts
- publishCount
- message

If TCP connection works but MQTT does not connect, check credentials, client ID, broker ACLs and listener configuration.
