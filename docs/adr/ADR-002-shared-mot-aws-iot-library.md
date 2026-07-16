# ADR-002 — Shared `MotAwsIot` firmware library

- **Status:** Accepted
- **Date:** 2026-07-16
- **Decision owners:** MOT maintainers
- **Related:** ADR-001, ADR-004, ADR-006, ADR-007

## Context

MOT supports more than one ESP32-based hardware platform:

- ESP32-WROOM
- LilyGO T-A7670X

Both platforms need the same production AWS behavior:

- LittleFS credential loading
- TLS/X.509 setup
- MQTT connection
- reconnect handling
- retained Last Will
- Birth messages
- heartbeat
- UTC validation
- topic construction
- common publish helpers

Maintaining independent AWS implementations in each firmware led to duplicate
logic, inconsistent diagnostics and a higher risk of fixing a bug on one board
but not the other.

## Decision

All stable production AWS IoT transport behavior is implemented once in the
shared PlatformIO library:

```text
firmware/shared-libs/MotAwsIot/
```

Board-specific firmware uses this library and provides only runtime metadata
and telemetry values.

The shared library owns:

- credential loading from `/aws` in LittleFS
- AWS endpoint, Thing name and vehicle ID parsing
- `WiFiClientSecure`
- `PubSubClient`
- TLS certificate configuration
- MQTT connection lifecycle
- retained Last Will
- retained Birth messages
- non-retained heartbeat
- UTC validity gating
- reconnect timing
- common topic construction
- typed publish helpers
- connection and diagnostic status

Board-specific code owns:

- hardware initialization
- CAN access and decoding
- GPS access
- Wi-Fi or LTE network management
- local WebUI
- OTA
- board-specific telemetry selection
- network availability and runtime metadata

## Design boundary

The shared library does not decode vehicle data and does not manage CAN, GPS or
the board WebUI.

Typical interaction:

```text
board network + telemetry
        |
        v
MotAwsIot runtime metadata and publish helpers
        |
        v
AWS IoT Core
```

## Alternatives considered

### Keep one AWS implementation per board

Rejected because it duplicates security-sensitive code and makes behavior drift
likely.

### Copy LilyGO code into ESP32-WROOM

Rejected because copying is not reuse. Future changes would still need to be
applied twice.

### Build a very broad hardware abstraction before sharing AWS logic

Rejected for the current stage because it would couple the AWS migration to a
larger firmware redesign. The shared library uses a narrower, validated
boundary.

### Move all telemetry mapping into the shared library

Deferred. A future shared telemetry model may be useful, but transport
consolidation was the immediate stable contract. Board-specific mapping remains
until the telemetry contract is further standardized.

## Consequences

### Positive

- one AWS security and connection implementation
- consistent Last Will, Birth and heartbeat behavior
- fixes apply to both boards
- fewer board-specific source files
- easier addition of future boards
- easier testing of the transport contract
- clearer separation between hardware and cloud concerns

### Negative

- shared library changes can affect multiple firmware targets
- integration testing is required on every supported board
- PlatformIO library path configuration must remain consistent
- board-specific needs may require carefully designed extension points
- the library currently assumes an ESP32/Arduino environment

## Compatibility

The production Wi-Fi AWS builds use `MotAwsIot`.

Legacy debug environments may still use their existing plain MQTT path.

For LilyGO, the AWS production build remains Wi-Fi based. Experimental LTE
behavior is intentionally separate until LTE MQTT stability is validated.

## Validation evidence

The shared library has been built and tested with:

- ESP32-WROOM AWS target
- LilyGO AWS target
- two separate device identities
- two vehicle IDs
- retained online state
- heartbeat
- last-seen timestamps
- current-state ingestion
- dashboard switching

## Follow-up

Potential future extensions:

- shared telemetry DTO/model
- injectable network client abstraction
- certificate rotation hooks
- Device Shadow support
- IoT Jobs/OTA hooks
- explicit unit tests for topic generation and credential parsing
