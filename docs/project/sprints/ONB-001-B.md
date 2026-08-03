# ONB-001.B — Controlled User and Device Onboarding

> **Status:** Active
>
> **Audience:** Maintainer, portal/backend developer and beta administrator
>
> **Last verified:** 2026-08-03 against branch `codex/spr-0005-beta-onboarding-readiness`

## Purpose

Build the controlled lifecycle that follows the completed ONB-001.A authorization
foundation. A beta administrator must be able to invite an account and bind one
portal vehicle identity without exposing device credentials or relying on the
firmware's local WebUI.

## Scope and slices

| Slice | Outcome | Status |
|---|---|---|
| ONB-001.B1 | Controlled invitation and administrator assignment | Implemented locally; apply validation pending |
| ONB-001.B2 | Expiring single-use claim proof and portal claim flow | Data model drafted; not implemented |
| ONB-001.B3 | Replacement, transfer, loss and recovery lifecycle | Design drafted; not implemented |

Public self-registration, billing, fleet-wide administration, firmware-local
onboarding and cloud OTA are outside ONB-001.B.

## Identity model

| Entity | Stable key | Authority |
|---|---|---|
| User | Cognito `sub` | Cognito |
| Portal vehicle/telemetry identity | `vehicleId` | MOT backend |
| Physical adapter | `deviceId` | Device inventory |
| Cloud device identity | Thing name + certificate | AWS IoT |

For the current product, an ESP32-WROOM, LilyGO or later adapter may be treated as
its own portal vehicle identity even when tested on the same physical Microlino.
Most users will have one assignment, but the authorization model continues to
support multiple vehicles per user.

An adapter replacement is not silently equivalent to an ownership transfer. B3
must explicitly decide whether to retain the existing `vehicleId` on the new
adapter or retire it and assign a new identity. Defect, loss and factory reset use
the same reviewed decision boundary. A factory reset never creates a second
unmanaged Thing or certificate.

## ONB-001.B1 trust boundary

The first slice uses a maintainer CLI, not a public portal administrator API. It:

- resolves the target stack and tables from CloudFormation outputs;
- validates email and `vehicleId` locally;
- confirms that telemetry state exists for the vehicle;
- checks Cognito and existing assignments;
- rejects another ACTIVE owner unless a later reviewed sharing model permits it;
- plans by default and requires explicit `--apply` for mutation;
- uses Cognito-generated invitation delivery and never handles a password;
- creates the assignment conditionally and records timestamps/source metadata;
- never prints email, full Cognito subject, token or device credential in its result.

Cognito and DynamoDB cannot be changed atomically together. If invitation succeeds
but assignment fails, the safe recovery state is an invited user with no access.
Repeating the command resumes from current state; it must not overwrite a REVOKED
assignment or reactivate access implicitly.

B1 assumes one active administrator execution at a time. Its read-before-write
owner check and conditional user-assignment write fail safely for normal retries,
but they are not a cross-user uniqueness transaction. B2 must introduce a canonical
vehicle-ownership/claim record that can be conditionally consumed atomically rather
than relying on a table scan.

## Claim-proof target for B2

The later portal claim uses at least 128 bits of cryptographic randomness. Only a
salted/domain-separated hash is stored. A claim record has explicit issue and
expiry times, attempt limits and `ISSUED`, `CONSUMED`, `REVOKED` or `EXPIRED` state.
Consumption and assignment creation must be one atomic backend transaction. The
proof is never placed in firmware logs, URLs, Git, screenshots or long-lived portal
storage.

The CloudFormation foundation template is already near the 51,200-byte inline API
limit. B2 backend code must use a separate onboarding stack/package boundary rather
than adding another inline Lambda to `cloud/aws/foundation/template.yaml`.

## Acceptance gates

- [x] B1 dry-run makes no cloud mutation and produces no sensitive output.
- [x] B1 invite/assign is locally tested as idempotent and fail-closed on conflict/revocation.
- [x] Invitation success plus assignment failure is safely resumable in an isolated test.
- [ ] A user with no assignment sees the onboarding-required portal state.
- [ ] B2 claim proof is expiring, single use, rate limited and stored only as a hash.
- [ ] Claim consumption and assignment are atomic.
- [ ] Replacement/transfer/recovery never reuses or exposes device credentials.
- [ ] Every production mutation produces privacy-safe audit evidence.

## Planned end-to-end beta validation

After implementation, Change Set review and explicit approval for destructive cloud
actions, use one controlled adapter currently assigned to `beta-01` as a clean
onboarding case. Before removing anything, inventory and record the exact Cognito
assignment, ownership, Thing, certificate, policy attachment and vehicle-state
targets. Deletion or deactivation must be limited to those reviewed identifiers and
must preserve unrelated users and the LilyGO path.

A second WROOM without GPS can validate multiple `vehicleId` assignments for one
user. Claiming, list isolation, stale/offline state and lifecycle recovery do not
require live CAN data. Tests that assert changing SOC/charging telemetry require one
selected adapter to be connected to the Microlino; normally the LilyGO may remain the
vehicle's operational adapter.

The validation sequence must separately approve account/assignment removal, IoT
certificate deactivation, credential provisioning and firmware upload. A clean
portal claim does not itself authorize any of those device operations.

## Stop gates

- no public self-registration;
- no real invitation or assignment during local B1 development;
- no new cloud stack before design and Change Set review;
- no certificate upload, rotation, revocation, firmware flash or factory reset;
- no production portal deployment until exact origin/callback/logout values exist.

## Related documents

- [ONB-001.A validation](../../testing/ONB-001-A-validation.md)
- [Authorization foundation](../../architecture/onboarding-authorization.md)
- [Administrator assignments](../../auth/admin/user-vehicle-assignments.md)
- [AWS IoT credentials](../../security/aws-iot-credentials.md)
- [Cloud risk register](../../security/cloud-risk-register.md)
- [B2 claim data model](../../architecture/onboarding-claim-data-model.md)
- [B3 device lifecycle](../../architecture/onboarding-device-lifecycle.md)
