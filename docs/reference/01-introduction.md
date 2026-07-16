# 01 — Introduction

[Next: Design principles →](02-design-principles.md)

## Vision

Microlino Open Telemetry (MOT) is an open telemetry platform for Microlino
vehicles.

The project connects vehicle data to a modular embedded adapter, a secure
cloud pipeline and a multi-vehicle web application. Its purpose is not merely
to forward CAN frames. MOT provides a stable integration boundary for vehicle
telemetry, device diagnostics, location, user applications and future
services.

## System at a glance

```text
Microlino
  -> CAN / OBD-II
  -> telemetry device
  -> MQTT/TLS
  -> AWS IoT Core
  -> current-state backend
  -> Vehicle API
  -> Dashboard and future applications
```

Two firmware platforms are currently supported:

- ESP32-WROOM
- LilyGO T-A7670X

Both use the shared `MotAwsIot` library for the production AWS Wi-Fi path.

## Project goals

MOT aims to provide:

- a hardware-independent telemetry model
- reproducible firmware builds
- secure device identity
- one cloud path for multiple adapter types
- multiple vehicles in one application
- a backend suitable for future user authorization
- a clear distinction between current state and history
- local operation even when cloud connectivity is unavailable
- an open foundation for GPS, OTA, ABRP and external integrations

## Current platform

### Device layer

The adapter reads vehicle data, maintains network connectivity and publishes
telemetry.

Device identity and vehicle identity remain separate:

```text
device / Thing: mot-esp32-...
vehicleId:      beta-01
```

A physical device can therefore be replaced without redefining the logical
vehicle throughout the application.

### Cloud layer

AWS IoT Core authenticates devices with X.509 certificates. An IoT Rule sends
messages to a Lambda function that stores the latest value per vehicle and
topic in DynamoDB.

The current-state key is:

```text
partition key: vehicleId
sort key:      topicSuffix
```

### Application layer

A read-only Vehicle API exposes:

```text
GET /api/vehicles
GET /api/vehicles/{vehicleId}/snapshot
```

The Dashboard uses these endpoints instead of connecting directly to MQTT.

This prepares the application for Cognito and user-to-vehicle authorization.

## Current maturity

The cloud foundation is operational and has been validated with:

- one LilyGO adapter
- one ESP32-WROOM adapter
- two separate AWS Things
- two vehicle IDs
- independent online state
- independent current-state snapshots
- Dashboard switching between vehicles

The platform is still a beta:

- the Vehicle API is not yet an authenticated end-user API
- GPS integration is still being expanded
- LTE-to-AWS transport is not yet the production path
- history and trip storage are not implemented
- OTA/device lifecycle work remains

## What MOT is not

MOT is not:

- a safety-critical vehicle control system
- a replacement for the Microlino vehicle controller
- a public API ready for untrusted users
- a time-series history platform yet
- a reason to store device private keys in source control

The system is read-oriented telemetry infrastructure. It must not be used to
control safety-relevant vehicle functions without a separate safety design.

## Reading path

Continue with:

- [Design principles](02-design-principles.md)
- [Terminology](03-terminology.md)
- [Documentation principles](../adr/ADR-000-documentation-principles.md)

The hardware, firmware, cloud, Dashboard, API, security and development
chapters will build on these foundations.
