# ADR-0002: Use JSON Backup/Restore

## Status

Accepted

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
