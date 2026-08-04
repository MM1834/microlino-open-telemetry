# Documentation Map

> **Status:** Current routing index
>
> **Audience:** Maintainer, contributor and automation agent

Read the authoritative governance set first, then select only the row matching the
task. Follow links from that owner page instead of scanning adjacent directories.

| Task | Start with | Add only when needed |
|---|---|---|
| Current state or priorities | `governance/CURRENT_STATUS.md`, `governance/WORK_ORDER.md` | `governance/ENGINEERING_BACKLOG.md` |
| Firmware behaviour | `firmware/README.md` | Relevant subsystem page and source files |
| Local WebUI | `webui/overview.md` | Relevant feature page and firmware route |
| Portal/dashboard | `dashboard/overview.md` | `architecture/authentication.md`, relevant portal source |
| AWS/backend | `architecture/aws-iot.md` | `api/`, `administrator/aws/`, relevant template or handler |
| Legacy MQTT forwarding | `tools/node-red-legacy-aws-forwarder.md` | AWS credential and onboarding docs |
| Authentication/onboarding | `architecture/onboarding-authorization.md` | Active ONB sprint, `auth/`, backend source |
| Hardware | `hardware/overview.md` | One board or wiring page |
| Build/release/deployment | `development/release-process.md` | Relevant build/deployment page and active release record |
| Validation | Relevant file under `testing/` | Exact sprint/release record |
| Historical audit | `history/README.md`, then `archive/README.md` | Only the named historical collection |

`archive/` is excluded from normal work. Historical documents do not override
current code, accepted ADRs or governance status.
