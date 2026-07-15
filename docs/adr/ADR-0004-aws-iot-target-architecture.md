# ADR-0004: AWS IoT target architecture

- **Status:** Accepted for implementation
- **Date:** 2026-07-15
- **Decision owners:** Microlino Open Telemetry maintainers
- **Scope:** Device-to-cloud communication, identity, authorization, WebApp access and future fleet operations

## Context

Microlino Open Telemetry currently publishes telemetry to a self-hosted MQTT broker. The end-user architecture still needs encrypted communication, unique device identities, revocable credentials, user/vehicle access control, scalable ingestion and a secure browser/mobile application path.

The LilyGO MQTT-over-LTE path is not yet stable. The cloud migration must therefore be validated first through WiFi and must not be combined with the LTE transport rewrite.

## Decision

MOT will use **AWS IoT Core** as the target device message broker and identity boundary.

### Device identity

Each beta device receives:

- one AWS IoT Thing,
- one unique Thing name,
- one unique X.509 client certificate,
- one unique private key,
- one least-privilege AWS IoT policy.

The MQTT client ID must equal the Thing name.

### Initial transport

```text
MQTT over TLS
AWS IoT Core data endpoint
TCP port 8883
X.509 mutual authentication
```

Reference sequence:

```text
ESP32-WROOM -> WiFi -> MQTT/TLS -> AWS IoT Core
LilyGO      -> WiFi -> MQTT/TLS -> AWS IoT Core
LilyGO      -> LTE  -> MQTT/TLS -> AWS IoT Core (later)
```

### Web application

The production WebApp will use an authenticated backend rather than device certificates or direct device-level credentials:

```text
Browser / Mobile WebApp
    -> authenticated HTTPS/WebSocket API
    -> application backend
    -> AWS IoT Core and telemetry storage
```

This supports user management, per-vehicle authorization and service ownership.

### Telemetry topics

The first AWS implementation preserves the current MOT topic structure to avoid changing transport, security and data model simultaneously:

```text
mot/<vehicleId>/display/...
mot/<vehicleId>/charging/...
mot/<vehicleId>/location/...
mot/<vehicleId>/system/...
mot/<vehicleId>/status/...
```

A future versioned namespace may use:

```text
mot/v1/<thingName>/telemetry/...
mot/v1/<thingName>/status/...
mot/v1/<thingName>/commands/...
```

### Presence

Device presence consists of:

- MQTT Last Will: `status/online = false`, retained,
- publish after connection: `status/online = true`, retained,
- periodic heartbeat,
- `system/last_seen_utc`,
- stable client ID equal to Thing name.

### Device Shadow

Use Shadows for compact state and desired configuration, not telemetry history.

Suitable values:

- firmware version,
- network mode,
- last-seen timestamp,
- configuration version,
- reporting interval,
- desired OTA version,
- summarized SOC.

Potential Named Shadows:

```text
status
configuration
ota
```

### Rules Engine and storage

```text
telemetry topics -> IoT Rule -> time-series/history storage
location topics  -> IoT Rule -> location/history storage
event topics     -> IoT Rule -> notification processing
```

The first sprint uses the AWS MQTT test client only. Storage is deferred.

### Provisioning

Beta phase:

- manual Thing creation,
- manual unique certificate per device,
- manual credential installation.

Future fleet phase:

- AWS IoT Fleet Provisioning,
- provisioning template,
- unique operational certificate after provisioning.

A shared operational certificate for multiple deployed devices is prohibited.

### OTA

Local WebUI OTA remains the supported method initially. AWS IoT Jobs is deferred until signed firmware, integrity validation, rollback and job reporting are available.

## Security requirements

1. Each physical device has an independent private key.
2. Private keys are never committed to Git.
3. AWS IoT policies use least privilege.
4. A device may publish only to its own telemetry/status namespace.
5. A device may receive only its own commands/configuration namespace.
6. Production WebApp users never receive device private keys.
7. Certificate revocation for one device must not affect other devices.
8. Device time must be valid before TLS validation.
9. TLS and LTE are introduced in separate validation stages.

## Consequences

### Positive

- managed MQTT/TLS broker,
- strong per-device identity,
- individual credential revocation,
- scalable ingestion and rules,
- clear path to user/vehicle authorization,
- future support for Shadow, Jobs and Fleet Provisioning.

### Negative

- AWS account and IAM complexity,
- certificate provisioning and secure storage,
- application backend required for end-user WebApp,
- cloud costs and operational monitoring,
- higher ESP32 flash/RAM use for TLS.

## Alternatives considered

### Self-hosted MQTT only

May remain supported for developers and self-hosting, but is not the target managed end-user architecture.

### Direct browser MQTT over WebSocket

Useful for prototypes, but not selected as the end-user architecture because MOT needs user-level authorization and vehicle ownership.

### AWS IoT FleetWise

Deferred. MOT currently needs secure MQTT telemetry rather than a full managed vehicle signal catalog.

### AWS IoT Greengrass

Rejected for the ESP32 device path because it is outside the hardware and project scope.

## Implementation phases

### AWS-1 — Architecture and manual provisioning

- account/Region selection,
- Thing naming,
- one test Thing and certificate,
- least-privilege policy,
- AWS MQTT test client.

### AWS-2 — ESP32-WROOM WiFi/TLS

- TLS client,
- credential storage,
- UTC validation,
- current topics,
- Last Will and heartbeat.

### AWS-3 — LilyGO WiFi/TLS

- same AWS identity model,
- memory and responsiveness validation,
- no LTE changes.

### AWS-4 — Backend and users

- authentication,
- user-to-vehicle ownership,
- HTTPS/WebSocket API,
- telemetry history.

### AWS-5 — LilyGO LTE/TLS

- only after LTE MQTT receive path is stable,
- modem/GPS/system time,
- reconnect and watchdog tests.

### AWS-6 — Fleet operations

- Fleet Provisioning,
- certificate rotation,
- AWS IoT Jobs,
- signed OTA.

## References

- AWS IoT Core protocols: https://docs.aws.amazon.com/iot/latest/developerguide/protocols.html
- X.509 client certificates: https://docs.aws.amazon.com/iot/latest/developerguide/x509-client-certs.html
- AWS IoT policies: https://docs.aws.amazon.com/iot/latest/developerguide/iot-policies.html
- Device Shadows: https://docs.aws.amazon.com/iot/latest/developerguide/iot-device-shadows.html
- AWS IoT Rules: https://docs.aws.amazon.com/iot/latest/developerguide/iot-rules.html
- Fleet Provisioning: https://docs.aws.amazon.com/iot/latest/developerguide/provision-wo-cert.html
- AWS IoT Jobs: https://docs.aws.amazon.com/iot/latest/developerguide/jobs-devices.html
