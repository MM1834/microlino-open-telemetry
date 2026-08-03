# Documentation Archive

> **Status:** Historical evidence
>
> **Audience:** Maintainer and auditor

This directory preserves superseded delivery packages, sprint records, release
artifacts, migration notes and other historical documentation. Files here may
accurately describe an earlier repository revision, but they are not current
implementation, deployment or security instructions.

## Use boundary

- Start normal work from `docs/governance/`, `docs/README.md` and the current
  topic documentation.
- Do not use archived instructions operationally without validating them against
  the current code, configuration and authoritative governance records.
- Consult this archive only when an authoritative document references it, when
  reconstructing history, or when performing an audit.
- Preserve release validation, recorded decisions, accepted risks and
  traceability. Moving a record into this directory does not change its meaning.

## Collections

- `aws-packages/` — historical AWS delivery packages and manifests
- `dashboard/` — historical dashboard increments and supporting notes
- `developer-history/` — historical LTE, MQTT, CAN and diagnostic investigations
- `documentation-migrations/` — documentation cleanup and migration packages
- `firmware/` — historical firmware fixes, experiments and delivery notes
- `legacy-root/` — obsolete root stubs and miscellaneous root fragments
- `releases/` — historical version and release artifacts
- `sprints/` — sprint manifests, patches, reviews, validation and integration
  records

## REL-001 boundary

The authoritative REL-001 release record remains in the active documentation
tree. Its sprint record, release notes, validation evidence and risk decisions
must remain semantically unchanged. Archive cleanup must not weaken or reinterpret
that evidence chain.
