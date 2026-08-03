# Engineering History Index

> **Status:** Current navigation to historical evidence
>
> **Audience:** Maintainer and auditor

Historical records explain delivery sequence and prior validation claims. They are
not current build, deployment, security or operating instructions unless a current
page explicitly revalidates them against an exact commit and environment.

## Repository history collections

- [AWS delivery increments](../aws/README.md)
- [Authentication increments](../auth/README.md)
- [Sprint records](../project/sprints/README.md)
- [Release and versioning index](../release/README.md)
- [Release-note archive](../release-notes/README.md)
- [Firmware release-note archive](../developer/release-notes/README.md)
- [Legacy documentation](../legacy/README.md)

## Root-level delivery packages

The repository root still contains historical manifests, changelogs, validation
records and release notes with names such as:

```text
AWS-*
README-AWS-*
SPR-*
CHANGELOG-*
GITHUB_RELEASE_NOTES*
RELEASE_CHECKLIST_*
```

They remain in place during DOC-001 to preserve links and audit context. Their
presence is not evidence that a validation command ran successfully or that its
described AWS state still exists. Moving them requires an inbound-link map and,
where rationale is unclear, Chat-export reconciliation.
