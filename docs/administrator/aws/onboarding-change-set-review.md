# ONB-001.B2 Change Set Review

> **Status:** Change Set available — not executed
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
| Execution | Not performed |

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

## Execution gates

Before execution:

- approve the fourteen additions and 90-day audit retention;
- accept that the manually bootstrapped artifact bucket is currently outside a
  CloudFormation stack and record its later lifecycle decision;
- do not add an administrator to `mot-beta-admins` until post-deploy verification is
  ready;
- do not remove `beta-01` or deactivate its certificate during stack creation;
- execute with rollback enabled, then verify effective IAM, CORS, TTL, encryption,
  API authorization and empty tables before any claim is issued.

## Related documents

- [ONB-001.B work package](../../project/sprints/ONB-001-B.md)
- [Claim data model](../../architecture/onboarding-claim-data-model.md)
- [Cloud read-only verification](read-only-verification.md)
