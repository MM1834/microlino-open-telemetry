# ADR-0002: Use JSON Backup/Restore

> **Classification:** Accepted; current local capability with security constraints
>
> **Audience:** Firmware maintainer and support

## Status

Accepted

Current export includes secrets. Apply the
[safe diagnostic-data rules](../beta/safe-diagnostic-data.md); acceptance of the
format is not approval to share an export through ordinary support channels.

## Context

The device stores multiple credentials and runtime configuration values.

## Decision

Use JSON export/import for configuration backup and restore.

## Consequences

Positive:

- Easy migration
- Easy recovery after factory reset
- Human-readable

Negative:

- Contains secrets
- Must be handled as sensitive data
