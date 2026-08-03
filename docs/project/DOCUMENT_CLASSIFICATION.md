# Documentation Classification Register

> **Status:** Active DOC-001 migration record
>
> **Audience:** Maintainer
>
> **Inventory date:** 2026-07-31

## Purpose

This register classifies the complete documentation tree by directory rule and
records exceptions for current sources. A directory rule covers every Markdown
file below that directory unless an exception is listed. This avoids maintaining a
fragile 244-row copy of the filesystem while still giving every document a status.

## Status vocabulary

| Status | Meaning |
|---|---|
| Current | Intended source for the current repository revision |
| Unverified | Intended current guidance, but runtime/deployment evidence is missing |
| Planned | Design or future capability not fully implemented |
| Historical | Describes an earlier sprint, release or implementation stage |
| Redirect | Compatibility entry pointing to a canonical source |
| Mixed | Directory requires file-level rules below |

## Complete directory classification

| Path | Files at inventory | Default status | Canonical role/action |
|---|---:|---|---|
| `docs/*.md` | 9 | Mixed | Navigation and compatibility redirects only |
| `administrator/` | 2 at baseline | Mixed | Historical auth notes plus current AWS as-is/verification sources |
| `adr/` | 12 | Mixed | Decision records; resolve numbering and supersession |
| `api/` | 4 | Unverified | Current contracts after code comparison |
| `architecture/` | 11 | Mixed | Current architecture except named sprint record |
| `assets/` | 2 Markdown | Current | Asset indexes; binary assets under `assets/images/` |
| `archive/` | Added after baseline | Historical | Consolidated root delivery packages; excluded from normal work |
| `beta/` | 7 | Mixed | Draft WROOM guide/checklists; safe-data handling baseline is current |
| `auth/` | 7 | Historical | Increment-specific Cognito notes; current index replaces status claims |
| `aws/` | 15 | Historical | AWS-1/AWS-2/AWS-3 delivery and validation evidence |
| `configuration/` | 6 | Unverified | Current configuration guides pending endpoint/runtime validation |
| `dashboard/` | 8 | Unverified | Current hosted UI feature reference; screenshots await revalidation |
| `developer/` | 41 at baseline | Current | Current engineering guides; historical investigations moved to archive |
| `development/` | 2 at baseline | Mixed | Current build, release, deployment and versioning procedures |
| `firmware/` | 43 at baseline | Mixed | Current subsystem pages; proven historical duplicates removed |
| `getting-started/` | 5 | Unverified | Candidate beta-user instructions pending device validation |
| `governance/` | 7 | Current | Governance, status, work and engineering memory |
| `gps/` | 2 | Historical | GPS sprint delivery evidence |
| `hardware/` | 14 | Unverified | Current hardware reference pending maintainer/electrical review |
| `history/` | 1 added by DOC-001.5 | Current | Navigation to retained historical evidence and `archive/` |
| `images/` | 10 Markdown at baseline | Removed | Obsolete stubs removed after zero-inbound-link verification |
| `legacy/` | 1 | Historical | Legacy index |
| `project/` | 10 | Historical by default | Sprint records; DOC-001 and this register are active exceptions |
| `reference/` | 5 | Current | Stable introduction, principles and terminology |
| `release/` | 2 at baseline | Removed | Current versioning moved to `development/`; checklist archived |
| `release-notes/` | 1 | Historical | Version record |
| `releases/` | 1 at baseline | Removed | Parallel summary removed; detailed note retained in `release-notes/` |
| `roadmap/` | 2 | Planned | Links to governed work/backlog and AWS phases |
| `security/` | 1 at baseline | Unverified | Current credential requirements and code-review risk register |
| `testing/` | 4 | Historical | Prior test records; no current-head validation suite |
| `troubleshooting/` | 1 | Unverified | Candidate current support reference |
| `user/` | 1 | Historical | Stale API-access increment note; portal onboarding docs planned |
| `user-guide/` | 4 | Planned | Beta-guide placeholders and redirects |
| `webui/` | 11 | Unverified | Current local-WebUI reference pending hardware/UI validation |

The inventory contains 244 Markdown files. Counts are a migration baseline and may
decrease as exact duplicates become redirects or history indexes.

After this baseline, historical root packages were moved without content changes
into `docs/archive/`. Two byte-identical redundant copies were removed after hash
and `cmp` verification; their canonical copies remain in the active or retained
history trees.

## Root exceptions

| File | Status | Canonical target |
|---|---|---|
| `README.md` | Current | Documentation landing page |
| `index.md` | Current | Audience-based navigation |
| `DOCUMENTATION_STANDARD.md` | Current | Documentation structure and style |
| `MIGRATION.md` | Current | DOC-001 baseline and deferred migration ledger |
| `ARCHITECTURE.md` | Redirect | `architecture/README.md` |
| `DASHBOARD.md` | Redirect | `dashboard/overview.md` and `webui/overview.md` |
| `firmware.md` | Redirect | `firmware/overview.md` |
| `developer.md` | Redirect | `developer/README.md` |

## Current architecture exceptions

Current:

- `architecture/README.md`
- `architecture/overview.md`
- `architecture/aws-iot.md`
- `architecture/authentication.md`
- `architecture/authentication-flow.md`
- `architecture/telemetry-data-flow.md`
- `architecture/can-profile-framework.md`
- board and ABRP architecture pages, currently Unverified

Historical:

- `architecture/SPR-0004B.8-portal-integration-preparation.md`

## Developer and firmware file rules

The following filename patterns are Historical regardless of directory:

- `v*.md` and `no-firmware-change-*.md`;
- `*-fix.md`, `*-cleanup.md`, `*-trace.md`, `*-debug.md`;
- `lilygo-lte-stack-v*.md`, `lilygo-lte-at-stack-v*.md`;
- transport experiment/migration pages describing TinyGSM or LewisXhe increments;
- all files under `archive/developer-history/release-notes/`.

Historical developer collections are indexed under `archive/developer-history/`.
Subsystem pages without a delivery/fix/version name are candidate current sources
and remain Unverified until code comparison.

## ADR classification rule

The [ADR index](../adr/README.md) owns current classification. Filenames, not bare
numbers, are stable identifiers because two generations overlap. DOC-001.5 records
the collision without renaming files or inventing missing supersession rationale.

## Canonical topic ownership

| Topic | Current owner | Historical/supporting sources |
|---|---|---|
| Project state and priorities | `governance/CURRENT_STATUS.md`, `WORK_ORDER.md` | sprint/release documents |
| System architecture | `architecture/overview.md` | Draw.io/SVG and older overview pages |
| AWS IoT and portal architecture | `architecture/aws-iot.md` | `aws/`, AWS ADRs |
| Browser authentication | `architecture/authentication.md` | `auth/`, SPR-0004B.1/.3 |
| Vehicle REST API | `api/vehicle-api.md` | AWS-3.3/.4, CloudFormation |
| Live WebSocket API | `api/live-websocket-api.md` | phase 4/4B records, CloudFormation |
| Local configuration API | `api/configuration-api.md` | B.8 record, firmware WebUI code |
| MQTT topics | `api/mqtt-topics.md` | firmware topic code, AWS-2.3 contract |
| AWS credentials | `security/aws-iot-credentials.md` | provisioning scripts and AWS-1 records |
| Declared AWS stack state | `administrator/aws/declared-stack-state.md` | CloudFormation and historical AWS records |
| Cloud risks/gaps | `security/cloud-risk-register.md` | code/configuration findings |
| Firmware | `firmware/overview.md` plus subsystem pages | developer investigations and release notes |
| Firmware/hardware gaps | `firmware/known-gaps.md` | source/configuration findings |
| Local WebUI | `webui/overview.md` plus feature pages | old screenshots/user guides |
| Portal UI | `dashboard/overview.md` plus feature pages | dashboard sprint records |
| Beta guide and support baseline | `beta/` draft documents | getting-started, user-guide and WebUI reference |
| DOC-001 validation and handover | `project/DOC-001-VALIDATION.md` | checks and unresolved gates |

## Chat-export reconciliation queue

- rationale and chronology behind duplicate ADR numbering;
- whether the two short authentication notes contain rationale not preserved in Git;
- intended targets of unresolved ADR-004 through ADR-008 references in ADR-001–003;
- which historical AWS deployments and validation checklists were actually run;
- final intended status of older LTE transport experiments;
- hardware rationale for current CAN wiring and planned model variants;
- which screenshots correspond to which firmware/dashboard revision;
- whether root-level delivery packages have external links that block relocation;
- any decisions made in chat but never promoted into repository ADRs.

## Related documents

- [DOC-001 sprint](sprints/DOC-001.md)
- [Migration ledger](../MIGRATION.md)
- [Documentation standard](../DOCUMENTATION_STANDARD.md)
