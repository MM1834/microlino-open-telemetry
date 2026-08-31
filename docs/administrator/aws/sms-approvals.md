# Controlled SMS administrator approvals

> **Status:** Active SMS-001.C operator runbook
>
> **Region:** `eu-north-1`

SMS delivery requires a separate administrator approval keyed by exact Cognito
subject and vehicle. The user-facing Notification Preference API has no IAM
permission to read or write this approval and cannot set an `smsApproved` field.

## Safety boundary

The approval record contains the normalized destination only as a SHA-256
fingerprint. The audit record also contains only that fingerprint. Plaintext
telephone numbers remain limited to the existing encrypted preference record and
the operator's transient CLI input.

The CLI assumes `mot-dev-sms-approval-admin`. That role is trusted only for the
declared maintainer principal and can:

- read one vehicle-access, preference or approval record;
- write/update only the SMS approval table;
- append only to the SMS approval audit table.

It cannot edit user preferences, vehicle access, telemetry, notification events,
AWS SMS configuration, spend limits or application enablement.

## Plan an approval

Omit `--phone-e164` so the number is entered through a non-echoing prompt and does
not enter shell history:

```sh
python3 tools/aws/admin_sms_approval.py approve \
  --user-sub '<cognito-sub>' \
  --vehicle-id '<vehicle-id>' \
  --expires-days 30 \
  --reason sms-001-controlled-pilot
```

Without `--apply`, the command is read-only. It validates an `ACTIVE` vehicle
assignment, CH `+41` or DE `+49` destination, exact `MOT` originator boundary and current
approval version. Output contains the destination fingerprint but neither phone
number nor Cognito subject.

After reviewing the plan, repeat the same command with `--apply`. Approval and
audit are written in one DynamoDB transaction with an optimistic version
condition. A concurrent or stale update fails without a partial audit record.

## Revoke an approval

Use `revoke` with the same identity and destination, first without and then with
`--apply`. Revocation requires a currently `ACTIVE` approval, increments its
version and writes the audit event atomically.

Changing a user's destination never transfers approval: the later dispatcher
must recompute the current destination fingerprint and require an exact match.
Expired, revoked, missing or unreadable approvals fail closed.

One mobile number may be used by several vehicles or users. AWS verification is
shared by its destination fingerprint, but administrator approval is not: run a
separate approval for every exact `userSub + vehicleId + fingerprint`
association. The portal shows only whether the caller's selected association is
ready and never reveals other users or vehicles attached to the same number.

CH and DE use separate exact sender identities and Protect rules. Never approve
the revoked `xrpioneer` assignment; the active Swiss xruser pilot vehicle is
`xrpioneer2` and expects a `+41` number. DE is reserved for a separate future
user. A DE approval may be planned only after that user is registered, has
entered and confirmed the `+49` number, and the exact verified destination ARN
has been added to the dispatcher policy.

## Current live resources

| Resource | Name | Boundary |
|---|---|---|
| Approval table | `mot-dev-sms-approvals` | encrypted, PAY_PER_REQUEST, `expiresAt` TTL |
| Audit table | `mot-dev-sms-approval-audit` | encrypted, PAY_PER_REQUEST, 90-day TTL |
| Admin role | `mot-dev-sms-approval-admin` | exact maintainer trust, table-only policy |

No real approval is created merely by deploying SMS-001.C. The SMS application
switch remains disabled until all later SMS-001 gates are accepted.

## Related documents

- [SMS-001 work package](../../project/sprints/SMS-001.md)
- [Notification email operations](email-delivery.md)
- [Current work order](../../governance/WORK_ORDER.md)
