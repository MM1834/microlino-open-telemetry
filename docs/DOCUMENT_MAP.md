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
| Cognito and notification email delivery | `administrator/aws/email-delivery.md` | `project/sprints/SES-001.md`, foundation and notification templates |
| Legacy MQTT forwarding | `tools/node-red-legacy-aws-forwarder.md` | AWS credential and onboarding docs |
| Authentication/onboarding architecture | `architecture/onboarding-authorization.md` | Active ONB sprint, `auth/`, backend source |
| Onboard a user, vehicle and adapter | `user/onboarding.md` | Device checklist, authorization architecture, active ONB sprint |
| Hardware | `hardware/overview.md` | One board or wiring page |
| Build/release/deployment | `development/release-process.md` | Relevant build/deployment page and active release record |
| Marketing and project poster | `marketing/project-poster.md` | `governance/CURRENT_STATUS.md`, `governance/WORK_ORDER.md` |
| Public landing page | `marketing/landing-page.md` | `project/sprints/WEB-001.md`, `development/release-process.md` |
| Validation | Relevant file under `testing/` | Exact sprint/release record |
| Historical audit | Git log for the relevant current owner path | Open the named older revision only |

Historical revisions do not override current code, accepted ADRs or governance
status.
