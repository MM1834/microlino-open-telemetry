# ADR-000 — Documentation principles

- **Status:** Accepted
- **Date:** 2026-07-16
- **Decision owners:** MOT maintainers

## Context

Microlino Open Telemetry has evolved from embedded firmware into a
multi-layer platform covering hardware, firmware, cloud infrastructure,
security, deployment tooling, APIs and a web application.

As the system grows, a single README and chronological sprint notes no longer
provide enough structure. Current behavior, historical development and the
reasoning behind architecture choices must remain distinguishable.

## Decision

MOT documentation is treated as a maintained project artifact and is divided
into four complementary forms.

### 1. Repository entry point

The root `README.md` is the concise public entry point. It explains:

- project purpose
- major capabilities
- supported platforms
- high-level architecture
- first links for readers
- current maturity and roadmap

The README must not become the complete system manual.

### 2. Current reference

`docs/reference/` describes the current supported system.

Reference documents explain what exists now and how it is used. They must not
mix current guidance with obsolete implementation history.

The planned reading order is:

```text
01 introduction
02 design principles
03 terminology
04 hardware
05 firmware
06 cloud
07 dashboard
08 API
09 security
10 development
11 roadmap
```

### 3. Architecture Decision Records

`docs/adr/` records important decisions and their rationale.

Each ADR contains:

- status
- context
- decision
- alternatives considered
- positive and negative consequences
- follow-up implications

Accepted ADRs are historical records. They are not silently rewritten when a
decision changes. A later ADR supersedes the earlier one and links back to it.

Typographical corrections, broken links and clarifications that do not alter
the decision are allowed.

### 4. Development history

Sprint notes, release notes and implementation-specific migration documents
remain available as historical material.

They explain how the project reached its current state, but are not the
authoritative operating reference.

## Diagram policy

Editable diagram sources are stored below `docs/architecture/`.

For every maintained diagram:

- the editable source is versioned
- an SVG export is committed for Markdown rendering
- a PNG export is optional
- labels use project terminology from the glossary
- diagrams avoid account IDs, real credentials and private endpoints

The editable source is authoritative when exports disagree.

## Screenshot policy

Screenshots are supporting evidence, not specifications.

Before committing a screenshot, verify that it exposes none of the following:

- private keys
- certificates
- AWS access keys
- passwords
- user email addresses
- personal identifiers
- internal host credentials
- secret query parameters

AWS account IDs, endpoint IDs and vehicle names should be redacted or replaced
when a screenshot is intended for public documentation and their presence adds
no explanatory value.

## Example and command policy

Examples must be safe to copy.

Use placeholders such as:

```text
<thing-name>
<vehicle-id>
<api-base-url>
```

Do not embed production credentials, local passwords or private keys.

Commands should state their expected working directory when ambiguity is
possible.

## Language and terminology

The primary technical documentation is written in English so the repository
can be used publicly. User-facing guides may later be translated.

Documents must use the glossary consistently. In particular:

- **vehicle** is the logical vehicle identity
- **device** is the physical telemetry adapter
- **Thing** is an AWS IoT resource
- **snapshot** is the latest known state of one vehicle
- **history** is time-series or trip data and is not part of current state

## Completion criteria

A feature is complete only when the relevant combination of these exists:

- implementation
- validation or tests
- reference documentation
- architecture update, when structure changes
- ADR, when a durable design decision is introduced
- release or migration notes, when operators must take action

Not every small fix requires a new ADR.

## Ownership and review

Documentation changes are reviewed with the same care as code changes.

Reviewers check:

- technical accuracy
- current commands and paths
- working internal links
- secret exposure
- terminology consistency
- screenshots and diagrams
- conflicts with accepted ADRs

## Consequences

### Positive

- New contributors have a predictable reading path.
- Durable design reasoning is preserved.
- Reference material remains focused on current behavior.
- Sprint history can remain detailed without confusing operators.
- Security-sensitive examples are handled consistently.

### Negative

- Features require additional documentation effort.
- Diagram exports must be kept synchronized.
- Maintainers must actively retire stale reference text.
- Changes that supersede decisions require new ADRs.

## Alternatives considered

### Keep all information in the root README

Rejected because the README would become too large and mix introduction,
reference, operations and history.

### Use only chronological sprint documents

Rejected because readers would need to reconstruct the current architecture
from historical changes.

### Rewrite ADRs whenever architecture changes

Rejected because it would erase the reasoning and context that led to earlier
decisions.

## Follow-up

The first ADR series will document:

- AWS IoT Core as the standard telemetry platform
- the shared `MotAwsIot` library
- REST instead of browser MQTT
- X.509 credentials in LittleFS
- DynamoDB as the current-state store
- the MQTT topic hierarchy
- hardware abstraction
- the cloud-first application boundary
