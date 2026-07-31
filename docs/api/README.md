# API Reference

> **Status:** Current index; runtime not revalidated
>
> **Audience:** Developer and administrator

## Hosted APIs

- [Vehicle REST API](vehicle-api.md)
- [Live WebSocket API](live-websocket-api.md)

These APIs require Cognito authentication for vehicle data, but per-user vehicle
authorization is not implemented.

## Device-local APIs

- [Configuration and readiness API](configuration-api.md)
- [Backup JSON](backup-json.md)
- [MQTT topics](mqtt-topics.md)
- [Legacy REST overview](rest-api.md)

Device-local endpoints are not a portal onboarding API and must not be exposed
directly to the public Internet.
