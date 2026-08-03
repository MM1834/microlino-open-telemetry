# API Reference

> **Status:** Current index; runtime not revalidated
>
> **Audience:** Developer and administrator

## Hosted APIs

- [Vehicle REST API](vehicle-api.md)
- [Live WebSocket API](live-websocket-api.md)

These APIs require Cognito authentication and enforce per-user vehicle access in
the validated development stack. ONB-001.A proved two-user REST/WebSocket isolation,
guessed-ID denial and live revocation/recovery. Production configuration and the
ONB-001.B claim lifecycle remain release gates.

## Device-local APIs

- [Configuration and readiness API](configuration-api.md)
- [Local device HTTP API](local-device-api.md)
- [Backup JSON](backup-json.md)
- [MQTT topics](mqtt-topics.md)
- [Legacy REST overview](rest-api.md)

Device-local endpoints are not a portal onboarding API and must not be exposed
directly to the public Internet.
