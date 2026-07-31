# SELF_REVIEW

**Project:** Microlino Open Telemetry (MOT)

**Document Type:** Governance

**Status:** Active

**Governance Version:** 1.0

**Maintainer:** Repository Maintainers

---

# Purpose

This document preserves engineering knowledge gained during the development of the Microlino Open Telemetry (MOT) repository.

Its purpose is to capture insights, observations and validated experience that may improve future engineering decisions.

Unlike technical documentation, this document records what was learned rather than what was implemented.

---

# Scope

SELF_REVIEW preserves engineering knowledge that remains valuable beyond the completion of individual development tasks.

Typical topics include:

- engineering observations
- validated assumptions
- unsuccessful approaches
- design trade-offs
- architectural experience
- repository evolution
- recurring engineering patterns

The document intentionally excludes implementation details, active work and future planning.

---

# Guiding Principle

Engineering experience is valuable only if it is preserved.

Knowledge gained during development should remain available to future maintainers so that successful decisions can be repeated and unsuccessful approaches are not unnecessarily revisited.

The objective is continuous engineering improvement through accumulated experience.

---

# Entry Criteria

An entry should only be created when it provides lasting engineering value.

Suitable examples include:

- significant engineering discoveries
- architectural lessons
- validated design decisions
- recurring implementation patterns
- important project insights
- knowledge that influenced future decisions

Routine implementation notes should remain within the relevant technical documentation.

---

# Recommended Entry Structure

Each entry should provide enough context to remain understandable over time.

## Title

A concise description of the engineering insight.

## Context

Which engineering problem or situation led to this experience?

## Observation

What was learned?

## Outcome

How did this knowledge influence the repository?

## Recommendation

How should future maintainers benefit from this experience?

---

# Maintenance

SELF_REVIEW should evolve continuously throughout the lifetime of the repository.

Entries should remain concise, factual and relevant.

Knowledge may be refined as additional engineering experience is gained, but historical observations should not be removed unless they are demonstrably incorrect or no longer applicable.

---

# What Does Not Belong Here

The following information should not be recorded in SELF_REVIEW:

- implementation tasks
- feature requests
- bug reports
- release notes
- project status
- future engineering ideas
- personal opinions without engineering evidence

The purpose of this document is to preserve engineering knowledge rather than project history.

---

# Relationship to Other Documents

PROJECT_CONSTITUTION defines engineering governance.

CURRENT_STATUS describes the current repository.

ENGINEERING_BACKLOG preserves future engineering opportunities.

WORK_ORDER describes current engineering activities.

SELF_REVIEW preserves engineering knowledge gained through development.

---

# Related Documents

- PROJECT_CONSTITUTION.md
- CURRENT_STATUS.md
- ENGINEERING_BACKLOG.md
- WORK_ORDER.md

---

# Preserved Engineering Lessons

## Authentication is not vehicle authorization

**Context:** Cognito login, JWT-protected REST routes and an authenticated WebSocket
were implemented before the ownership model.

**Observation:** A valid user token proves identity but does not determine which
vehicle that user may list, read or subscribe to.

**Outcome:** Portal onboarding remains blocked for multiple untrusted users until
REST and WebSocket handlers enforce a server-side user-to-vehicle relationship.

**Recommendation:** Test every vehicle access path with an authenticated but
unauthorized user, not only with missing or invalid tokens.

## Historical delivery records are not current status

**Context:** Two documentation generations, sprint packages, patch manifests and
roadmaps remained visible together after development moved between chats.

**Observation:** Individually accurate historical documents can contradict the
current code when they are presented without lifecycle labels.

**Outcome:** Governance status and work-order documents are now the primary current
status sources; sprint and release material is treated as history.

**Recommendation:** Record validation against an exact commit and move completed
delivery documents out of current navigation instead of rewriting history.

## Build environments should describe products, not implementation history

**Context:** Separate pre-AWS, AWS and GPS-test PlatformIO environments accumulated
while capabilities were introduced incrementally.

**Observation:** Once AWS IoT and optional GPS are normal capabilities, maintaining
them as firmware generations increases support and validation combinations without
adding product value.

**Outcome:** The target model is one maintained firmware line per board with
explicit feature/configuration choices.

**Recommendation:** Keep special test environments temporary and document their
retirement criteria when introduced.
