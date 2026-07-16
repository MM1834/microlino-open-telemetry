# MOT documentation

## Start here

1. [Introduction](reference/01-introduction.md)
2. [Design principles](reference/02-design-principles.md)
3. [Terminology](reference/03-terminology.md)

## Architecture decisions

- [ADR-000 — Documentation principles](adr/ADR-000-documentation-principles.md)
- [ADR-001 — AWS IoT Core as the standard telemetry platform](adr/ADR-001-aws-iot-core.md)
- [ADR-002 — Shared `MotAwsIot` firmware library](adr/ADR-002-shared-mot-aws-iot-library.md)
- [ADR-003 — Dashboard uses REST instead of browser MQTT](adr/ADR-003-dashboard-rest-api.md)

## Architecture

- [Architecture diagrams](architecture/README.md)
- [System overview](architecture/system-overview.svg)
- [Firmware architecture](architecture/firmware.svg)
- [AWS cloud architecture](architecture/aws-cloud.svg)
- [Dashboard data flow](architecture/dashboard.svg)

## Documentation status

This is package 2 of the `v0.9.1` documentation sprint.

Completed:

- documentation principles
- introduction
- design principles
- terminology
- first three platform ADRs
- current system, firmware, AWS and Dashboard diagrams

Next packages add:

- credential, current-state, topic and hardware-abstraction ADRs
- hardware and firmware reference
- cloud and API reference
- security and development guides
- roadmap and historical-document cleanup
