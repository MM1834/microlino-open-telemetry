# Documentation Migration

> **Status:** Active

## Goal

Consolidate two documentation generations and historical delivery packages into
the structure defined by [DOCUMENTATION_STANDARD.md](DOCUMENTATION_STANDARD.md).
Migration preserves useful engineering evidence while ensuring only one source is
presented as current for each topic.

## Migration rules

1. Compare a document with current code and configuration.
2. Move durable current knowledge into its canonical page.
3. Add a status banner and links to retained historical material.
4. Move sprint, patch and release narration under `history/` in a later mechanical
   pass after inbound links are mapped.
5. Delete only exact duplicates, generated metadata or content fully preserved by
   Git history and an approved canonical replacement.

## Current canonical areas

| Topic | Canonical location | Migration state |
|---|---|---|
| Governance and current priorities | `governance/` | Current |
| Architecture decisions | `adr/` | Needs numbering/status cleanup |
| System architecture | `architecture/` | Active consolidation |
| Firmware reference | `firmware/` | Active consolidation |
| Local device UI | `webui/` | Structurally current; screenshots await revalidation |
| Hosted portal/dashboard | `dashboard/` and `user/` | Must be updated with onboarding work |
| AWS implementation history | `aws/` | Historical; current design moves to architecture/admin/API |
| Authentication operations | `auth/`, `administrator/` | Needs role-based consolidation |
| Hardware | `hardware/` | Current base; vehicle/CAN variants incomplete |
| LTE diagnostics | `developer/lte/` | Historical investigation knowledge |
| Sprint records | `project/sprints/` | Historical |
| Release/version notes | `release-notes/`, `releases/` | Historical |
| Images | `assets/images/` | Canonical; duplicate tree being removed |

## Known duplicate generations

- top-level `ARCHITECTURE.md`, `DASHBOARD.md`, `firmware.md` and `developer.md`
  overlap with directory-based documentation;
- `docs/images/` duplicates many canonical files in `docs/assets/images/`;
- `firmware/` and `developer/` contain parallel copies of LTE, CAN, MQTT and
  operations notes;
- `aws/`, root sprint manifests and `project/sprints/` describe delivery stages,
  not the current service contract;
- `releases/`, `release-notes/` and developer release notes overlap.

## Screenshot backlog

Refresh screenshots only after validation of:

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
