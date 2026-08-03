# ONB-001.B2 Change Set Review

> **Status:** Executed successfully; post-deploy empty-state validation passed
>
> **Environment:** Development, `eu-north-1`
>
> **Reviewed:** 2026-08-03

## Prepared artifact

The dedicated bucket `mot-dev-artifacts-002581114110-eu-north-1` was created because
the account had no existing artifact bucket. Effective controls were read back after
creation:

- all four S3 Public Access Block settings are enabled;
- default and object encryption use SSE-S3 AES-256;
- bucket versioning is enabled;
- the uploaded object metadata records source commit `bdcae84` and SHA-256
  `e43586add7747678ffe3ba5316041a95322d5e59efd56ef025c22fc716a4e45f`;
- the returned full-object checksum matches the local package.

Artifact key:

```text
onboarding/bdcae84/onboarding-lambda-e43586add7747678ffe3ba5316041a95322d5e59efd56ef025c22fc716a4e45f.zip
```

## Change Set

| Field | Value |
|---|---|
| Stack | `mot-dev-onboarding` |
| Change Set | `onb-001-b2-preview-20260803` |
| Type | `CREATE` |
| Status | `CREATE_COMPLETE` / `AVAILABLE` |
| Capability | `CAPABILITY_IAM` |
| Execution | Completed 2026-08-03 |

Reviewed parameters include exact development CORS origin
`http://localhost:8080`, 24-hour claim validity, five attempts, seven-day Lambda
log retention and 90-day audit retention. Cognito issuer/client/pool and existing
state/access table names resolve to the current `mot-aws-3-1` foundation outputs.

## Planned resource changes

CloudFormation reports fourteen `Add` actions and no modification, replacement or
deletion:

- three encrypted pay-per-request DynamoDB tables: claims, ownership and audit;
- one packaged Python 3.13 Lambda and a dedicated least-privilege execution role;
- one seven-day CloudWatch log group;
- one HTTP API, JWT authorizer, Lambda integration, default stage and invoke
  permission;
- two JWT-protected POST routes for controlled issue and consumption;
- one Cognito group `mot-beta-admins` with no automatic member assignment.

The API allows only the exact local development origin and is throttled to rate 2,
burst 5. The Change Set does not alter the foundation stack, existing Cognito users,
user/vehicle assignments, IoT Things, certificates, firmware or portal files.

## Post-deploy evidence

Stack `mot-dev-onboarding` reached `CREATE_COMPLETE`. All fourteen resources report
`CREATE_COMPLETE`; the API output is
`https://3izicgmdxi.execute-api.eu-north-1.amazonaws.com`.

Effective-state checks confirmed:

- claims, ownership and audit tables are ACTIVE, encrypted and PAY_PER_REQUEST;
- consistent scans returned zero items for all three tables;
- claims and audit TTL use attribute `ttl` and report `ENABLED`;
- Lambda is Active, Python 3.13, 192 MB, ten-second timeout and its deployed code
  checksum matches the uploaded package;
- Lambda logs retain seven days and contain no data before invocation;
- both POST routes use the Cognito JWT authorizer;
- the default stage enforces rate 2 and burst 5;
- anonymous claim consumption returns HTTP 401 before Lambda invocation;
- localhost preflight returns only the exact configured CORS origin; an unrelated
  origin receives no access-control headers;
- inline data policy is limited to the five named DynamoDB tables;
- `mot-beta-admins` contains no users;
- no claim, ownership or audit record was created during validation.

## Next mutation gates

Before functional claim testing:

- accept that the manually bootstrapped artifact bucket is currently outside a
  CloudFormation stack and record its later lifecycle decision;
- explicitly approve adding one controlled administrator to `mot-beta-admins`;
- inventory `beta-01` before approving assignment removal or certificate action;
- test authenticated non-admin denial before issuing the first real claim.

## Related documents

- [ONB-001.B work package](../../project/sprints/ONB-001-B.md)
- [Claim data model](../../architecture/onboarding-claim-data-model.md)
- [Cloud read-only verification](read-only-verification.md)
