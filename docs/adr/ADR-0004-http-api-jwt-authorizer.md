# ADR-0004 — HTTP API JWT Authorizer

> **Status:** Historical implementation note; identifier collision
>
> **Audience:** Backend maintainer and auditor
>
> **Introduced:** 2026-07-20 Git history

The HTTP API validates Cognito JWTs through a managed JWT authorizer.

This short note collides with
[ADR-0004-aws-iot-target-architecture](ADR-0004-aws-iot-target-architecture.md).
The filename, not the bare number, is its stable identifier. Current behaviour and
security gaps are documented in the
[authentication architecture](../architecture/authentication.md).
