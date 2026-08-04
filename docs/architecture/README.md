# Architecture

> **Status:** Current navigation
>
> **Audience:** Developer and maintainer
>
> **Last verified:** 2026-07-31 against repository configuration; runtime not revalidated

## Principles

- passive vehicle integration;
- shared telemetry model and reusable services;
- platform-specific hardware behind narrow interfaces;
- configuration before firmware forks;
- local device operation remains available when cloud services fail;
- device identity and human-user identity are separate trust domains.

## Current architecture

- [System overview](overview.md)
- [Telemetry data flow](telemetry-data-flow.md)
- [Firmware architecture](../firmware/architecture.md)
- [AWS IoT and portal architecture](aws-iot.md)
- [Telemetry history pilot](telemetry-history.md)
- [Authentication](authentication.md)
- [CAN profile framework](can-profile-framework.md)

## Architecture decisions

Accepted and superseded decisions are indexed under [ADR](../adr/README.md).
Sprint-specific architecture documents are historical delivery records and should
not be used as current status without comparison to the links above.
