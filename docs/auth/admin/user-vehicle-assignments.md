# User-to-vehicle assignment operations

> **Status:** Controlled beta administrator procedure
>
> **Scope:** Development/beta assignments only; no public self-service claiming

## Safety boundary

Use the Cognito `sub` as the user key and the application `vehicleId` as the
vehicle key. Never use an email address, username, Thing name or certificate ID as
a substitute. Resolve the target stack, region, table, subject and vehicle with
read-only commands before making a change. Do not place tokens, passwords or device
credentials in assignment records or command transcripts.

Each change requires a reviewed operator ticket or work-package reference. Record
`createdAt`, `updatedAt` and a non-personal `source`. The current roles are `OWNER`
and, when later required, explicitly designed support roles; an unknown role must
not be invented during an incident.

## Read-only verification

Obtain the table name from the selected stack:

```bash
aws cloudformation describe-stacks \
  --stack-name <STACK_NAME> \
  --region <REGION> \
  --query "Stacks[0].Outputs[?OutputKey=='UserVehicleAccessTableName'].OutputValue | [0]" \
  --output text
```

Resolve the Cognito subject through the approved user-pool administration process,
then verify the intended vehicle ID against the state/device inventory. Query the
exact subject before changing it:

```bash
aws dynamodb query \
  --table-name <ACCESS_TABLE> \
  --key-condition-expression 'userSub = :subject' \
  --expression-attribute-values '{":subject":{"S":"<COGNITO_SUB>"}}' \
  --region <REGION>
```

## Create an assignment

Create only if the composite key does not already exist. Replace every placeholder
locally and review the final command before execution:

```bash
aws dynamodb put-item \
  --table-name <ACCESS_TABLE> \
  --item '<REVIEWED_ASSIGNMENT_JSON>' \
  --condition-expression 'attribute_not_exists(userSub) AND attribute_not_exists(vehicleId)' \
  --region <REGION>
```

The reviewed JSON contains only `userSub`, `vehicleId`, `status=ACTIVE`,
`role=OWNER`, ISO-8601 `createdAt`/`updatedAt`, and `source`. A failed condition means
the record already exists: stop and inspect it; do not overwrite it blindly.

## Revoke access

Prefer an auditable status transition over deletion. Update the exact composite key
to `REVOKED`, set `updatedAt`, and require the old status to be `ACTIVE`. Then verify
that REST access is denied and that the live fan-out removes an existing connection.
Physical device credentials are a separate identity boundary and are not rotated by
this operation.

## Required post-change evidence

- query the exact subject and confirm vehicle, status and role;
- test the assigned or revoked REST path with a controlled account;
- test the WebSocket subscription and fan-out behaviour;
- record stack, region, Git revision, operator reference and result without secrets;
- escalate any cross-user disclosure and block beta release.

## Related documents

- [User-pool operations](user-pool-operations.md)
- [Authorization foundation](../../architecture/onboarding-authorization.md)
- [ONB-001.A validation](../../testing/ONB-001-A-validation.md)
- [Live WebSocket API](../../api/live-websocket-api.md)
