# Documentation

> **Status:** Current navigation
>
> **Audience:** All readers

This directory contains current product documentation and retained engineering
history for Microlino Open Telemetry.

## Start here

- [Governance and handover](governance/README.md)
- [Validated current status](governance/CURRENT_STATUS.md)
- [Active work](governance/WORK_ORDER.md)
- [Architecture](architecture/README.md)
- [Firmware overview](firmware/overview.md)
- [ESP32-WROOM beta documentation](beta/README.md)
- [AWS IoT roadmap](roadmap/aws-iot.md)
- [Documentation standard](DOCUMENTATION_STANDARD.md)
- [Migration tracker](MIGRATION.md)
- [Documentation classification](project/DOCUMENT_CLASSIFICATION.md)
- [Architecture decisions](adr/README.md)
- [Engineering history](history/README.md)
- [DOC-001 validation and handover](project/DOC-001-VALIDATION.md)

## Documentation lifecycle

Documentation is being consolidated after two development generations were kept in
parallel. Until that work is complete, use this order of authority:

1. current code and configuration;
2. accepted current ADRs;
3. governance `CURRENT_STATUS` and `WORK_ORDER`;
4. current architecture and operator documentation;
5. release notes, sprint documents, manifests and patch packages as history.

Historical documents may accurately describe an earlier revision without
describing the current repository. They must not be used as deployment or security
instructions unless revalidated.

## Main sections

- `getting-started/` — installation and first use
- `beta/` — draft WROOM handoff, provisioning, support and release gates
- `architecture/` and `adr/` — system design and decisions
- `firmware/`, `webui/`, `dashboard/` — component documentation
- `aws/`, `auth/`, `security/` — cloud and identity
- `administrator/` — code-based service state and approved operational procedures
- `developer/`, `testing/` — engineering and validation
- `reference/` — stable terminology and contracts
- `release-notes/`, `releases/`, `project/sprints/`, `legacy/` — history
- `history/` — current navigation to retained historical collections

Each current topic should have one source of truth. Other documents should link to
it instead of maintaining a second status description.
