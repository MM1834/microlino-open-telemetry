# REL-001 Release-candidate Validation

> **Result:** Internal release-candidate gates passed
>
> **Date:** 2026-08-03
>
> **Validated reconciled develop commit:** `1e78197`

## Repository tests

| Suite | Result |
|---|---:|
| `tools/tests` | 72 passed |
| `cloud/aws/foundation/tests` | 9 passed |
| `cloud/aws/onboarding/tests` | 6 passed |
| Documentation audit | Passed |

## Firmware compile gates

| Environment | Result | RAM | Program flash |
|---|---:|---:|---:|
| `esp32dev-aws` | Passed | 50,864 / 327,680 bytes (15.5%) | 1,147,329 / 1,310,720 bytes (87.5%) |
| `T-A7670X-AWS` | Passed | 53,992 / 327,680 bytes (16.5%) | 1,146,613 / 1,310,720 bytes (87.5%) |

The code tree at `1e78197` is identical to the physically validated feature tree.
The reconciliation merge retained current `develop` content and only connected the
previously separate `main` Governance-1.0 ancestry.

## Git review gates

- PR #1 merged SPR-0005/DOC-001/ONB-001 and firmware readiness into `develop` as
  merge commit `d03fc10`;
- PR #2 reconciled the accidental `main`-only history as merge commit `1e78197`;
- PR #2 reported zero changed files, zero additions and zero deletions;
- after PR #2, `main` is an ancestor of `develop` with no commits remaining only on
  `main`;
- CLOUD-017 has an explicit invitation-only pilot acceptance and remains open for
  durable provider remediation before public rollout.

## Remaining release operations

- perform the final hosted `/motbeta/` smoke test against the intended portal files;
- review the `develop` to `main` promotion PR;
- do not delete or overwrite `/dashboard/` during pilot promotion;
- create a release tag only after the hosted smoke test and maintainer approval.

## Pilot support and device boundary

The repository/hosting maintainer is the first-line support and escalation owner
for REL-001. The validated pilot identities are:

- `beta-01`: existing ESP32-WROOM/GPS path;
- `beta-02`: freshly erased and provisioned ESP32-WROOM without GPS;
- `pioneer`: LilyGO T-A7670G with AWS IoT over WiFi/LTE and external GPS.

Adding another device requires the provisioning checklist, a unique certificate,
an explicit user/vehicle assignment plan and a new handoff record.

## Manual portal rollback

- keep the existing `/dashboard/` server directory untouched during the pilot;
- before replacing hosted pilot files, download or rename the current `/motbeta/`
  directory as a server-side/operator backup without committing its `config.js`;
- if the new upload fails, restore that complete directory through FileZilla FTPS;
- retain the currently registered Cognito callback/logout URL during rollback;
- confirm login, assigned-vehicle isolation and logout after restoration;
- firmware recovery remains USB-first and is independent of portal rollback.

## Related records

- [REL-001](../project/sprints/REL-001.md)
- [Device field validation](SPR-0005-device-field-validation.md)
- [Pilot release-note draft](../release-notes/REL-001-pilot-draft.md)
