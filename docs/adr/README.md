# Architecture Decision Records

> **Status:** Current decision index
>
> **Audience:** Maintainer, developer and architecture reviewer

## Identifier policy

Two ADR generations used overlapping number formats. Filenames are now the stable
identifier: a bare number such as “ADR-0004” is ambiguous and must not be used in
new documentation. Existing files are not renamed during DOC-001 because that
would break historical references and could imply a chronology not supported by
the available evidence.

New ADRs must use the next identifier assigned by maintainers after this collision
is resolved. Do not infer the next number from either historical sequence.

## Current and accepted decisions

| Stable document identifier | Status and current relevance |
|---|---|
| [ADR-000-documentation-principles](ADR-000-documentation-principles.md) | Accepted documentation policy |
| [ADR-001-aws-iot-core](ADR-001-aws-iot-core.md) | Accepted production telemetry platform |
| [ADR-002-shared-mot-aws-iot-library](ADR-002-shared-mot-aws-iot-library.md) | Accepted shared firmware transport design |
| [ADR-003-dashboard-rest-api](ADR-003-dashboard-rest-api.md) | Accepted portal/backend boundary |
| [ADR-0004-aws-iot-target-architecture](ADR-0004-aws-iot-target-architecture.md) | Accepted target architecture; overlaps and expands ADR-001 |
| [ADR-0002-backup-json](ADR-0002-backup-json.md) | Accepted local capability with sensitive-data risk |

Acceptance records a decision; it does not prove current-head build, hardware or
deployed-cloud validation. Current implementation state belongs in architecture,
firmware and operations documentation.

## Retained historical or superseded-context records

| Stable document identifier | Classification |
|---|---|
| [ADR-0001-use-lewisxhe-tinygsm](ADR-0001-use-lewisxhe-tinygsm.md) | Accepted for the legacy LilyGO LTE/MQTT path; not the AWS-over-LTE target |
| [ADR-0004-http-api-jwt-authorizer](ADR-0004-http-api-jwt-authorizer.md) | Historical implementation note; collides with AWS target ADR |
| [ADR-Authentication-Strategy](ADR-Authentication-Strategy.md) | Historical implementation note without a numbered decision record |

The two authentication notes are represented by the current
[authentication architecture](../architecture/authentication.md). They remain for
traceability until the ChatGPT Classic export confirms whether additional rationale
must be preserved.

## Known integrity issues

- references inside ADR-001 through ADR-003 to ADR-004 and later numbers were
  written before matching records existed; they are not resolvable links;
- the `ADR-0004` collision was introduced on different dates for different topics;
- no supersession metadata was recorded for the short authentication notes;
- old `CURRENT.md` and `LEGACY.md` pages are compatibility redirects to this index.

These issues are recorded rather than silently repaired. Final numbering and
supersession rationale remain Chat-export reconciliation items.
