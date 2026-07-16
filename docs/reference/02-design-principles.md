# 02 — Design principles

[← Introduction](01-introduction.md) · [Next: Terminology →](03-terminology.md)

These principles guide implementation and review. They are not substitutes
for ADRs; ADRs record specific durable decisions.

## Secure production path

Production telemetry uses authenticated and encrypted transport.

Current implementation:

```text
MQTT over TLS
X.509 client certificate
AWS IoT Core
```

Plain MQTT is retained only as an explicit local development/debug option.

## One identity per physical device

Each telemetry adapter receives its own:

- AWS IoT Thing
- client certificate
- private key
- MQTT client ID

A certificate is not shared across physical devices.

This makes individual revocation, replacement and auditing possible.

## Device identity is not vehicle identity

The physical adapter and logical vehicle are modeled separately.

```text
thingName / clientId -> physical device
vehicleId            -> application and routing identity
```

Replacing an adapter should not force an application-wide vehicle rename.

## Shared before duplicated

Cross-board production behavior belongs in shared libraries when its contract
is stable.

The `MotAwsIot` library centralizes:

- credential loading
- TLS configuration
- AWS MQTT connection
- reconnect handling
- Last Will and Birth messages
- heartbeat
- topic construction
- common publish helpers

Board code remains responsible for hardware and telemetry acquisition.

## Cloud transport is internal

Applications do not need device MQTT credentials.

The Dashboard consumes a Vehicle API rather than connecting directly to AWS
IoT Core. This creates a security and product boundary suitable for:

- user login
- vehicle authorization
- rate limiting
- API evolution
- mobile applications

## Current state before history

The first cloud store answers:

> What is the latest known value for this vehicle?

It does not attempt to preserve every message forever.

History, trips and analytics are separate concerns and require explicit
retention, cost and privacy decisions.

## Vehicle-first routing

Telemetry topics use:

```text
mot/<vehicleId>/<category>/<signal>
```

This allows cloud processing to partition state by vehicle while keeping the
device identity independent.

## Offline-tolerant firmware

Cloud connectivity must not be required for core vehicle data acquisition.

CAN decoding, local diagnostics and local configuration remain useful when
AWS is unavailable.

## API before application coupling

Backend contracts are defined before UI-specific logic.

The Dashboard should render a stable vehicle snapshot instead of reproducing
broker semantics in every client.

## No secrets in the repository

The public project must not contain:

- device private keys
- client certificates
- AWS access keys
- passwords
- live broker credentials

Device material is staged under an ignored top-level `private/` directory and
uploaded to LittleFS during provisioning.

## Observable failure modes

Network, TLS, MQTT, time synchronization and cloud state must expose
diagnostics sufficient to distinguish:

- Wi-Fi unavailable
- time invalid
- credentials missing
- TLS/MQTT connect failure
- authorization failure
- stale vehicle data
- device offline
- API unavailable

## Prefer reproducible infrastructure

AWS resources are created through versioned templates and scripts rather than
manual console-only procedures.

Manual console views are used for inspection and diagnosis, not as the sole
record of infrastructure.

## Evolve in validated increments

Major layers are validated independently:

1. device TLS connection
2. MQTT and policy behavior
3. presence and heartbeat
4. ingestion
5. current-state storage
6. API
7. Dashboard
8. user authorization

This reduces the number of layers debugged at the same time.
