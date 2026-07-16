# 03 — Terminology

[← Design principles](02-design-principles.md)

This glossary defines the preferred terms used in code, documentation and
architecture discussions.

| Term | Meaning |
|---|---|
| **MOT** | Microlino Open Telemetry. The complete project and platform. |
| **Vehicle** | A logical Microlino identity represented by `vehicleId`. It is the primary application entity. |
| **Vehicle ID** | Stable routing and storage identifier such as `pioneer` or `beta-01`. Used in MQTT topics and API paths. |
| **Device** | A physical telemetry adapter, for example one ESP32-WROOM or LilyGO board installed in a vehicle. |
| **Device ID** | Firmware-visible hardware identity, typically derived from board-specific identifiers. |
| **AWS IoT Thing** | AWS registry resource representing one physical device. |
| **Thing name** | Name of the AWS IoT Thing. The production MQTT client ID is normally identical. |
| **Client ID** | MQTT connection identifier. It must be unique among concurrent clients and is restricted by the device policy. |
| **Certificate** | X.509 client certificate used by AWS IoT Core to authenticate one device. |
| **Private key** | Secret key belonging to the device certificate. It must never be committed or shared. |
| **Root CA** | Trusted Amazon root certificate used by the device to verify the AWS IoT endpoint. |
| **Telemetry** | Measurements or state published by a device, such as SOC, network mode or location. |
| **Signal** | One telemetry value, typically represented by one MQTT topic suffix. |
| **Category** | First component below a vehicle topic, for example `system`, `display`, `charging`, `location` or `status`. |
| **Topic suffix** | Path below `mot/<vehicleId>/`, for example `system/last_seen_utc`. |
| **Birth message** | State published immediately after a successful MQTT connection, including `status/online=true`. |
| **Last Will** | MQTT message registered during connection and published by the broker after an unexpected disconnect. MOT uses retained `status/online=false`. |
| **Heartbeat** | Periodic non-retained JSON diagnostic message indicating that the device and MQTT session are active. |
| **Last seen** | Device-provided UTC timestamp of the latest regular telemetry update. |
| **Current state** | Latest stored value per vehicle and topic suffix. |
| **Snapshot** | API response containing the current state of one vehicle at request time. |
| **History** | Persisted time-ordered telemetry, trips or analytics. Not part of the current-state store. |
| **Ingestion** | Cloud processing that receives MQTT messages and updates backend storage. |
| **Vehicle API** | HTTP API providing vehicle lists and current-state snapshots. |
| **Dashboard** | Browser application that consumes the Vehicle API and renders the selected vehicle. |
| **Provider** | Dashboard component that supplies normalized vehicle data. Production uses the AWS backend provider. |
| **Legacy MQTT** | Optional browser or firmware debug path using a conventional MQTT broker. It is not the production architecture. |
| **LittleFS** | On-device filesystem used to store provisioned AWS credentials separately from source code. |
| **Provisioning** | Creating cloud identity, placing credentials in the private local store and uploading them to a device. |
| **Cloud foundation** | AWS IoT Core, IoT Rule, Lambda, DynamoDB current state and Vehicle API established in the `v0.9.x` milestone. |
| **Multi-vehicle** | Ability to ingest, store, list and display more than one `vehicleId`. |
| **Multi-user** | Future authenticated ability to restrict users to assigned vehicles. Multi-vehicle does not automatically imply multi-user security. |

## Naming rules

Recommended examples:

```text
Thing / client ID: mot-lilygo-fe8ce0
Vehicle ID:        pioneer
MQTT topic:        mot/pioneer/system/last_seen_utc
API path:          /api/vehicles/pioneer/snapshot
```

Use lowercase, URL-safe vehicle IDs. Avoid spaces and personally identifying
names in public examples.

## Important distinction

These concepts must not be used interchangeably:

```text
user != vehicle != device != AWS IoT Thing
```

A future user may access multiple vehicles. A vehicle may receive a
replacement device. Each device retains its own cloud identity.
