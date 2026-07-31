# Governance

This directory contains the governance framework for the Microlino Open Telemetry (MOT) repository.

The governance documents define how engineering knowledge is organized, preserved and maintained throughout the lifetime of the project.

Their purpose is to provide future maintainers with a consistent and long-term engineering framework rather than implementation-specific documentation.

---

# Governance Documents

| Document | Purpose |
|----------|---------|
| PROJECT_CONSTITUTION.md | Defines the governance principles of the repository. |
| PROJECT_HANDOVER.md | Introduces the project, its objectives and engineering philosophy. |
| CURRENT_STATUS.md | Provides a validated snapshot of the repository's current state. |
| ENGINEERING_BACKLOG.md | Preserves long-term engineering opportunities that are intentionally deferred. |
| WORK_ORDER.md | Describes the engineering work that is currently planned or in progress. |
| SELF_REVIEW.md | Preserves engineering knowledge and experience gained during development. |

---

# Knowledge Architecture

The governance documents are intentionally separated according to their individual responsibilities.

```
Governance
        │
        ▼
Project Orientation
        │
        ▼
Current Repository State
        │
        ├──────────────┐
        ▼              ▼
Active Work     Engineering Opportunities
        │              │
        └──────┬───────┘
               ▼
      Engineering Memory
```

Each document has a single responsibility.

Together they establish the long-term knowledge architecture of the repository.

---

# Governance Principles

The governance framework is based on the following principles:

- documentation is an engineering artifact
- knowledge should remain inside the repository
- governance evolves more slowly than the repository
- maintainability takes precedence over complexity
- each document has a clearly defined responsibility

---

# Governance Lifecycle

```
Draft
   │
   ▼
Review
   │
   ▼
Repository Ready
   │
   ▼
Maintenance
```

Governance documents are expected to evolve through maintenance rather than continuous redesign.

Architectural changes should be exceptional and may require a future governance version.

---

# Repository

Microlino Open Telemetry (MOT)

Governance Version: **1.0**

The governance framework was restored to the active `develop` branch on
2026-07-31. Governance release history is currently represented by Git tags and
commit history; there is no separate `RELEASE.md` yet.
