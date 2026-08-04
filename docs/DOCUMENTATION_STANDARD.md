# Documentation Standard

> **Status:** Current
>
> **Audience:** Documentation maintainer and contributor

## Purpose

MOT documentation uses one predictable structure so current instructions are not
confused with implementation history. New and migrated pages must follow this
standard.

## Information architecture

| Directory | Audience and responsibility |
|---|---|
| `governance/` | Maintainers: status, active work, backlog and engineering memory |
| `getting-started/` | Beta users: installation, first start and essential setup |
| `user/` | Portal users: accounts, onboarding and API access |
| `webui/` | Device-local WebUI operation and recovery |
| `dashboard/` | Portal/dashboard features visible to users |
| `hardware/` | Supported boards, wiring, installation and electrical constraints |
| `firmware/` | Current firmware behaviour and subsystem reference |
| `configuration/` | Supported configuration and backup procedures |
| `api/` | Current local and cloud API contracts |
| `architecture/` | Current system architecture and data flows |
| `adr/` | Accepted and superseded architecture decisions |
| `security/` | Trust boundaries, credentials and security procedures |
| `administrator/` | Production/beta service operation |
| `developer/` | Current engineering guides |
| `development/` | Current build, release and deployment procedures |
| `testing/` | Repeatable validation plans and evidence |
| `assets/images/` | Canonical image and diagram assets |

Completed sprints, patch packages, superseded release notes and legacy delivery
records belong in Git history rather than the current documentation tree.

## Required page header

Every current page should start with:

```markdown
# Page title

> **Status:** Current
>
> **Audience:** Beta user | Administrator | Developer | Maintainer
>
> **Last verified:** YYYY-MM-DD against commit `<hash>` or Not yet revalidated
```

Historical pages use:

```markdown
> **Status:** Historical — describes `<release/sprint>`; not current guidance.
```

Planned designs use:

```markdown
> **Status:** Planned — not implemented.
```

## Recommended current-page structure

Use only the sections that add value, in this order:

1. Purpose
2. Scope
3. Prerequisites or trust boundary
4. Current behaviour or procedure
5. Validation/status
6. Troubleshooting or limitations
7. Related documents

Avoid changelog narration in current reference pages. Preserve it in `history/`.

## Source-of-truth rules

- One current page owns each topic.
- Related pages link to the owner instead of copying its content.
- Code/configuration takes precedence over stale prose.
- A historical validation statement is not evidence for a newer commit.
- Planned capabilities must not be written in the present tense.
- Local WebUI and portal instructions remain separate because they have different
  trust boundaries.

## Images and diagrams

- Store canonical assets only under `docs/assets/images/`.
- Use `hardware/`, `webui/`, `dashboard/`, `architecture/` and `branding/`
  subdirectories.
- Prefer Mermaid for flows that can be maintained as text.
- Keep Draw.io source beside exported SVG for diagrams requiring a visual editor.
- Give screenshots descriptive names based on function, not sprint number.
- A screenshot must record its source revision or verification date in the page.
- Do not publish screenshots containing tokens, endpoint secrets, email addresses,
  device certificates, private keys, WiFi credentials or identifiable beta data.

Portal and local-WebUI screenshots will be refreshed after those interfaces are
revalidated. Existing screenshots may remain as historical visual references but
must not be used as proof of the current UI.

## Language and naming

Current technical reference is written in English so identifiers and code terms
remain consistent. Beta-user support material may additionally be translated.
Use these product terms consistently:

- **local WebUI** for the browser interface hosted by a device;
- **portal** for the hosted user-facing website;
- **device** for a physical telemetry unit;
- **vehicle** for the Microlino associated with a device;
- **Thing** only for the AWS IoT resource.
