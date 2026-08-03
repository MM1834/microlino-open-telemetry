# Engineering History Index

> **Status:** Current navigation to historical evidence
>
> **Audience:** Maintainer and auditor

Historical records explain delivery sequence and prior validation claims. They are
not current build, deployment, security or operating instructions unless a current
page explicitly revalidates them against an exact commit and environment.

## Repository history collections

- [Consolidated documentation archive](../archive/README.md)
- [AWS delivery increments](../aws/README.md)
- [Authentication increments](../auth/README.md)
- [Sprint records](../project/sprints/README.md)
- [Current release process](../development/release-process.md)
- [Firmware versioning](../development/firmware-versioning.md)
- [Release-note archive](../release-notes/README.md)
- [Firmware development history](../archive/developer-history/)
- [Legacy documentation](../legacy/README.md)

## Archived root-level delivery packages

Historical root manifests, changelogs, validation records and release notes have
been grouped under `docs/archive/` in collections such as:

```text
AWS-*
README-AWS-*
SPR-*
CHANGELOG-*
GITHUB_RELEASE_NOTES*
RELEASE_CHECKLIST_*
```

Their preservation is not evidence that a validation command ran successfully or
that its described AWS state still exists. The archive preserves their content
and Git traceability while keeping them outside normal current-documentation work.
