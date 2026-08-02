# DOC-001 Validation and Handover Report

> **Status:** Complete static validation; runtime and maintainer gates remain open
>
> **Audience:** Maintainer, reviewer and next-sprint owner
>
> **Validation date:** 2026-08-02
>
> **Source baseline:** `15da2bd`; DOC-001.6 documentation-only normalization

## Outcome

DOC-001 establishes a navigable, code-aligned documentation baseline and is ready
for maintainer review. It does not approve a beta firmware release, AWS deployment,
hardware connection or portal-onboarding implementation by itself.

## Static checks

| Check | Result | Evidence |
|---|---|---|
| Repository state before DOC-001.6 | Pass | Clean branch tracking its remote at `15da2bd` |
| Markdown inventory | Observed | 423 Markdown files across repository and retained history |
| Local Markdown links | Pass | 0 unresolved local targets |
| Current navigation graph | Pass | 92 reachable pages; 0 missing Status/Audience after normalization |
| Embedded images | Pass | 32 references; all resolve below `docs/assets/images/` |
| Old `docs/images/` use | Pass with retained history | 10 historical Markdown files; 0 inbound Markdown links |
| Secret-path tracking | Partial; new gap recorded | Only placeholders are tracked, but WROOM `data/aws` staging lacks an explicit ignore rule |
| Whitespace/error check | Pending final commit | `git diff --check` is run before commit and repeated afterward |

The checks are static filesystem and Git inspections. No dependency installation,
build, upload, device command, network request or AWS action was performed.

## Acceptance criteria

| DOC-001 criterion | Result | Qualification |
|---|---|---|
| Reachable pages have Status and Audience | Pass | Measured from `docs/index.md` and `docs/README.md` |
| One declared current owner per topic | Pass | Canonical ownership table is in the classification register |
| Current claims are sourced or marked unverified | Pass for navigated baseline | Runtime-dependent pages are explicitly Unverified |
| Portal onboarding is not described as implemented | Pass | Authentication exists; claim/assignment remains Planned |
| Local WebUI and portal responsibilities are separate | Pass | Local API is not the portal onboarding boundary |
| Device identity and user authorization are separate | Pass | X.509 Thing identity is distinct from user-to-vehicle access |
| Beta instructions protect secrets | Pass | Support rules prohibit requesting keys, tokens and unredacted exports |
| Images use canonical asset tree | Pass | 32 of 32 embedded image references use canonical assets |
| Internal Markdown links resolve | Pass | 0 broken local links across 423 Markdown files |
| `git diff --check` | Pending final commit | Must pass again on the committed validation tree |
| Validation is commit-specific | Pass after final record | Exact validated commit is appended after the first closeout commit |
| Historical conflicts are queued | Pass | ADR, AWS, LTE, hardware and screenshot questions are recorded |

## Open implementation and release gates

These findings are documented and intentionally not resolved by DOC-001:

- no current-head PlatformIO build or automated firmware test;
- no ESP32-WROOM hardware, CAN, GPS, local-WebUI or OTA validation;
- no read-only deployed-AWS inventory or end-to-end X.509 telemetry validation;
- missing user-to-vehicle authorization and device claim lifecycle;
- open local AP/WebUI and configuration-sensitive OTA protection;
- AWS readiness false positive and legacy-MQTT System Health diagnostic;
- factory reset retains LittleFS AWS identity;
- ESP32-WROOM temporary `data/aws` credential files are not explicitly ignored;
- beta wiring, provisioning, recovery and support require maintainer review;
- current screenshots remain deferred until exact UI/device workflows are validated.

The authoritative implementation findings remain in the
[firmware gap register](../firmware/known-gaps.md),
[cloud risk register](../security/cloud-risk-register.md) and
[beta release-readiness checklist](../beta/release-readiness-checklist.md).

## ChatGPT Classic export reconciliation boundary

The export is required before destructive historical cleanup or invented rationale,
not before bounded new engineering. Reconcile it before:

- renumbering or merging colliding ADRs;
- deleting or relocating root sprint/AWS/release packages with unknown external links;
- asserting that historical AWS validation steps actually ran;
- finalizing old LTE transport intent or disputed hardware rationale;
- assigning old screenshots to a firmware or portal revision;
- removing an apparently duplicate record whose decision context is absent from Git.

The export is not required to design ONB-001 from current code and explicit risks,
run separately approved builds, perform read-only AWS inventory, or execute bounded
hardware validation. New evidence must be recorded against its exact commit and
environment.

## Recommended next gate

Maintainers review this report, the WROOM provisioning checklist and the cloud/
firmware blockers. The next sprint can then choose between:

1. an evidence sprint for reproducible WROOM builds and controlled device checks;
2. ONB-001 portal authorization/onboarding design and implementation;
3. a deliberately parallel plan with explicit owners and shared release gates.

No beta handoff should occur until both the device evidence gate and the portal
authorization decision are satisfied or a narrowly bounded trusted-user exception
is explicitly accepted.
