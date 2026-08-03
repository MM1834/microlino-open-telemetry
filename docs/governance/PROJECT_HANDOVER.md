# PROJECT_HANDOVER

**Project:** Microlino Open Telemetry (MOT)

**Document Type:** Governance

**Status:** Active

**Audience:** Maintainer and contributor

**Governance Version:** 1.0

**Maintainer:** Repository Maintainers

---

# Purpose

This document introduces the Microlino Open Telemetry (MOT) project to future maintainers.

Its purpose is to provide the long-term context required to understand the project's objectives, engineering philosophy, repository organization and overall direction before contributing to the codebase.

Unlike technical documentation or project status reports, this document focuses on enduring project identity rather than implementation details or current development activities.

---

# Project Vision

Microlino Open Telemetry aims to provide an open, maintainable and extensible telemetry platform for the Microlino vehicle.

The project promotes transparent engineering, modular software design and reusable components while preserving long-term maintainability and engineering knowledge.

---

# Project Goals

The primary goals of the repository are:

- collect vehicle telemetry without influencing vehicle behaviour
- provide reliable telemetry acquisition across supported hardware platforms
- separate platform-specific implementations from reusable engineering components
- support integration with external services and automation platforms
- preserve engineering knowledge for future maintainers

The project values maintainability, transparency and engineering quality over rapid feature growth.

---

# Project Scope

The repository focuses on telemetry acquisition, processing and distribution.

Typical responsibilities include:

- vehicle communication
- CAN bus integration
- signal decoding
- GNSS positioning
- cellular and Wi-Fi connectivity
- telemetry transport
- configuration management
- platform abstraction
- authenticated portal access and telemetry services

The repository does not aim to modify vehicle control systems or influence vehicle behaviour.

Telemetry is intentionally passive unless explicitly documented otherwise.

The local firmware WebUI and the portal have different trust boundaries. The local
WebUI is for device-local setup, diagnostics, recovery and OTA. User accounts,
device claiming, vehicle ownership and fleet administration belong in the portal
and its backend, not in an Internet-facing firmware UI.

---

# Repository Organization

The repository is organized around reusable engineering components.

Whenever practical, functionality should be implemented within shared libraries rather than duplicated across hardware platforms.

Platform-specific implementations should primarily assemble reusable components while minimizing platform-dependent code.

---

# Platform Strategy

The project is designed to support multiple hardware platforms over time.

Platform support should be achieved through well-defined abstractions rather than independent implementations.

Platform-specific functionality should remain isolated behind stable interfaces whenever practical.

The maintained product direction is one firmware line per supported board.
Connectivity and optional hardware capabilities should be configuration or feature
choices, not permanently forked firmware generations.

---

# Shared Library Philosophy

Reusable functionality belongs in shared libraries.

Typical examples include:

- vehicle interfaces
- telemetry processing
- connectivity
- configuration handling
- utility components

The objective is to maximize reuse while minimizing maintenance effort.

---

# Telemetry Pipeline

The repository follows a modular telemetry pipeline.

```text
Vehicle Data
      │
      ▼
Signal Processing
      │
      ▼
Normalization
      │
      ▼
Telemetry Generation
      │
      ▼
Distribution
      │
      ▼
External Systems
```

Each stage should have a clearly defined responsibility.

The pipeline should remain understandable, modular and extensible.

---

# Configuration Philosophy

Configuration should remain external whenever practical.

Routine deployment changes should not require firmware modifications.

Configuration mechanisms should remain consistent across supported platforms.

---

# Engineering Workflow

Engineering work follows an iterative process.

```text
Observe
    │
    ▼
Understand
    │
    ▼
Validate
    │
    ▼
Implement
    │
    ▼
Review
    │
    ▼
Preserve Knowledge
```

Engineering knowledge gained during development should remain available within the repository.

---

# Governance Overview

Repository governance consists of six complementary documents.

| Document | Responsibility |
|----------|----------------|
| PROJECT_CONSTITUTION | Governance principles |
| PROJECT_HANDOVER | Project orientation |
| CURRENT_STATUS | Current repository state |
| ENGINEERING_BACKLOG | Long-term engineering opportunities |
| WORK_ORDER | Active engineering work |
| SELF_REVIEW | Engineering memory |

---

# First Hour Guide

New maintainers are encouraged to explore the repository in the following order:

1. PROJECT_CONSTITUTION
2. PROJECT_HANDOVER
3. CURRENT_STATUS
4. WORK_ORDER
5. ENGINEERING_BACKLOG
6. SELF_REVIEW

After completing these documents, the repository structure and engineering philosophy should be understandable before exploring the implementation.

Current follow-up work should then be selected from `WORK_ORDER` and approached
through the task-oriented [documentation map](../DOCUMENT_MAP.md). REL-001 and the
documentation baseline are complete; remaining work includes beta support
readiness, lifecycle recovery, firmware-environment simplification and extended
LilyGO qualification.

---

# Related Documents

- PROJECT_CONSTITUTION.md
- CURRENT_STATUS.md
- ENGINEERING_BACKLOG.md
- WORK_ORDER.md
- SELF_REVIEW.md
