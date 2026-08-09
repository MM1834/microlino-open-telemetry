# Documentation

> **Status:** Current navigation
>
> **Audience:** All readers

This directory contains current product, operating and release documentation for
Microlino Open Telemetry. Superseded delivery packages are retained by Git history,
not duplicated in the current working tree.

## Start here

- [Governance and handover](governance/README.md)
- [Validated current status](governance/CURRENT_STATUS.md)
- [Active work](governance/WORK_ORDER.md)
- [Architecture](architecture/README.md)
- [Authentication architecture](architecture/authentication.md)
- [Benutzer-, Fahrzeug- und Adapter-Onboarding](user/onboarding.md)
- [API reference](api/README.md)
- [Firmware overview](firmware/overview.md)
- [ESP32-WROOM beta guide](beta/esp32-wroom-guide.md)
- [AWS IoT roadmap](roadmap/aws-iot.md)
- [Documentation standard](DOCUMENTATION_STANDARD.md)
- [Task-oriented documentation map](DOCUMENT_MAP.md)
- [Architecture decisions](adr/README.md)
- [Developer build guide](development/build.md)
- [AWS declared-state operations](administrator/aws/declared-stack-state.md)
- [Stable terminology](reference/03-terminology.md)
- [Active work packages](project/sprints/README.md)
- [v1.0.0-rc.1 release notes](release-notes/v1.0.0-rc.1.md)
- [Project poster and website summary](marketing/project-poster.md)
- [Public landing page](marketing/landing-page.md)

## Documentation lifecycle

Use this order of authority:

1. current code and configuration;
2. accepted current ADRs;
3. governance `CURRENT_STATUS` and `WORK_ORDER`;
4. current architecture and operator documentation;
5. current release notes and active work packages.

Historical documents may be recovered from Git when needed. They must not be used
as deployment or security instructions unless revalidated.

## Main sections

- `getting-started/` — installation and first use
- `beta/` — draft WROOM handoff, provisioning, support and release gates
- `architecture/` and `adr/` — system design and decisions
- `firmware/`, `webui/`, `dashboard/` — component documentation
- `aws/`, `auth/`, `security/` — cloud and identity
- `administrator/` — code-based service state and approved operational procedures
- `developer/`, `development/`, `testing/` — engineering, procedures and validation
- `reference/` — stable terminology and contracts
- `release-notes/` — one versioned record per maintained release candidate/release
- `marketing/` — public high-level summaries derived from validated project status
- `project/sprints/` — active work packages only

Each current topic should have one source of truth. Other documents should link to
it instead of maintaining a second status description.
