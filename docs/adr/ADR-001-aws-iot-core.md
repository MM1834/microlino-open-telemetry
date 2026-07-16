# ADR-001 — AWS IoT Core as the standard telemetry platform

- **Status:** Accepted
- **Date:** 2026-07-16
- **Decision owners:** MOT maintainers
- **Related:** ADR-003, ADR-004, ADR-005, ADR-006

## Context

MOT originally used conventional MQTT brokers for development and dashboard
integration. That approach was useful for early testing, but it created several
limitations for the product direction:

- browser clients needed direct broker access
- device identity and authorization were loosely coupled
- multi-vehicle and future multi-user access were difficult to control
- TLS and certificate provisioning were not part of the standard workflow
- scaling the system beyond one local installation would require additional
  broker, API and security design

The project needs a secure production telemetry path that supports individual
device identities, per-device revocation, cloud-side routing and future
integration with authentication, storage, OTA and device lifecycle services.

## Decision

AWS IoT Core is the standard production telemetry platform for MOT.

Each physical telemetry adapter receives:

- one AWS IoT Thing
- one X.509 client certificate
- one private key
- one MQTT client ID matching the Thing name
- one least-privilege IoT policy

Production telemetry is published using MQTT over TLS to AWS IoT Core.

The standard device-to-cloud path is:

```text
MOT device
  -> MQTT/TLS
  -> AWS IoT Core
  -> IoT Rule
  -> Lambda
  -> DynamoDB current state
  -> Vehicle API
```

A conventional unencrypted MQTT path may remain available as a clearly marked
local debug mode, but it is not the production architecture.

## Requirements addressed

- encrypted transport
- mutual device authentication
- independent device revocation
- per-device policy enforcement
- multi-vehicle routing
- cloud-native ingestion
- compatibility with future Cognito, IoT Jobs and Device Shadow work
- no device credentials in the browser

## Alternatives considered

### Self-hosted Mosquitto as the production broker

Advantages:

- simple
- inexpensive for small installations
- familiar tooling
- easy local debugging

Rejected as the standard production platform because MOT would still need to
design and operate its own:

- certificate lifecycle
- authorization model
- high availability
- public endpoint hardening
- backend API boundary
- multi-user access control
- device fleet operations

Mosquitto remains useful for development and diagnostics.

### Direct HTTPS telemetry from devices

Advantages:

- familiar request/response model
- easy integration with typical web backends

Rejected because MQTT provides better semantics for constrained devices,
retained state, Last Will, long-lived connectivity and topic-based routing.

### Browser connection directly to AWS IoT Core

Advantages:

- live updates
- less backend code for an early prototype

Rejected as the production application model because browser authorization and
vehicle access would become tightly coupled to IoT permissions. It would also
expose cloud-MQTT concerns to every client application.

## Consequences

### Positive

- devices use a managed and secure MQTT service
- each adapter has a revocable identity
- retained state and Last Will work consistently
- cloud ingestion is reproducible with infrastructure as code
- application clients are separated from device credentials
- the architecture is ready for authenticated multi-user access
- future AWS IoT services can be added without replacing the transport

### Negative

- provisioning is more complex than plain MQTT
- every device requires certificate lifecycle management
- AWS service costs and quotas must be monitored
- incorrect IoT policies can disconnect devices unexpectedly
- cloud availability becomes part of the production telemetry path
- local/offline operation still requires separate firmware behavior

## Operational implications

Policies must explicitly allow retained publishing where retained messages are
used:

```text
iot:Publish
iot:RetainPublish
```

The MQTT client ID is restricted to the device Thing name.

Credentials are stored outside source control and uploaded to LittleFS during
provisioning.

## Validation evidence

The decision has been validated with:

- LilyGO T-A7670X over Wi-Fi
- ESP32-WROOM over Wi-Fi
- separate Things and certificates
- retained `status/online`
- MQTT Last Will
- heartbeat and last-seen topics
- multi-vehicle ingestion and dashboard selection

## Follow-up

Future work should add:

- Cognito and user-to-vehicle authorization
- production API protection
- device certificate rotation
- AWS IoT Jobs for OTA/lifecycle management
- evaluation of LTE transport to AWS IoT
