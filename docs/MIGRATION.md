# Documentation Migration

> **Status:** Structural consolidation complete; screenshot refresh remains open
>
> **Audience:** Documentation maintainer
>
> **Completed sprint:** [DOC-001](project/sprints/DOC-001.md)
>
> **Branch:** `codex/doc-001-documentation-baseline`

## Goal

Consolidate two documentation generations and historical delivery packages into
the structure defined by [DOCUMENTATION_STANDARD.md](DOCUMENTATION_STANDARD.md).
Migration preserves useful engineering evidence while ensuring only one source is
presented as current for each topic.

DOC-001 is the official execution vehicle for this migration. This page remains
the operational ledger; the sprint document owns scope, acceptance criteria and
review gates.

The complete directory rules and source ownership are recorded in the
[documentation classification register](project/DOCUMENT_CLASSIFICATION.md).

## Migration rules

1. Compare a document with current code and configuration.
2. Move durable current knowledge into its canonical page.
3. Add a status banner and links to retained historical material.
4. Keep sprint, patch and release narration under `archive/` after mapping inbound
   links; exclude that tree from normal current-documentation work.
5. Delete only exact duplicates, generated metadata or content fully preserved by
   Git history and an approved canonical replacement.

## Current canonical areas

| Topic | Canonical location | Migration state |
|---|---|---|
| Governance and current priorities | `governance/` | Current |
| Architecture decisions | `adr/` | Collision classified; final numbering awaits reconciliation |
| System architecture | `architecture/` | Current structure established |
| Firmware reference | `firmware/` | Source-based core consolidated; runtime unverified |
| Local device UI | `webui/` | Structurally current; screenshots await revalidation |
| Hosted portal/dashboard | `dashboard/` and `user/` | Must be updated with onboarding work |
| AWS implementation history | `aws/` | Historical index added; current architecture/API separated |
| Authentication and AWS operations | `auth/`, `administrator/`, `security/` | As-is reference, risks and read-only verification established |
| Hardware | `hardware/` | Current base; vehicle/CAN variants incomplete |
| ESP32-WROOM beta/support | `beta/` | Source-based draft complete; build/device/AWS validation open |
| LTE diagnostics | `archive/developer-history/lte/` | Historical investigation knowledge archived |
| Sprint records | `project/sprints/` | Historical |
| Release process and versioning | `development/` | Current procedures |
| Release notes | `release-notes/` | REL-001 plus retained historical notes |
| Historical navigation | `history/README.md`, `archive/README.md` | Root packages archived by coherent collection |
| Images | `assets/images/` | Canonical; obsolete `docs/images/` Markdown removed |

## Resolved duplicate generations

- old `docs/images/` Markdown stubs were removed after confirming canonical assets
  and zero inbound links;
- historical LTE, CAN, MQTT, operations and firmware release notes were moved from
  `developer/` to `archive/developer-history/`;
- corresponding unlabelled `firmware/` copies were removed only when their full
  body was preserved under a historical banner;
- `release/` and `releases/` were consolidated into current `development/`
  procedures, canonical `release-notes/` and the archive;
- historical root packages were grouped under `archive/`.

DOC-001.5 kept non-identical parallel release records and retained root manifests
until inbound links were mapped. The later mechanical consolidation moved those
packages into `docs/archive/`; ambiguous records remain preserved there pending
Chat-export reconciliation.

## Screenshot backlog

The detailed capture matrix is in the
[beta screenshot specification](beta/screenshot-specification.md). Refresh
screenshots only after validation of:

- ESP32-WROOM local WebUI with and without GPS;
- LilyGO local WebUI over the supported WiFi path;
- portal login, account invitation and logout;
- device claim/onboarding;
- vehicle list and authorization failures;
- support/diagnostic workflow;
- local OTA and, later, remote OTA.

Each screenshot task should specify viewport, test data and redaction requirements.

## Completion criteria

- every navigated page has a status and audience;
- one current source exists per topic;
- all current links resolve;
- historical pages are visibly historical;
- `docs/images/` is no longer used;
- screenshots match a verified release;
- current status contains no claims supported only by old sprint notes.
