# AWS Onboarding Boundary

> **Status:** Implemented locally for B2 — no onboarding stack is deployed

AWS CloudFormation `validate-template` accepted `template.yaml` in `eu-north-1`
on 2026-08-03. The result requires `CAPABILITY_IAM` because the stack declares a
dedicated Lambda role. Validation created no Change Set or resource.

This directory owns the future ONB-001.B2 claim and B3 lifecycle packaged backend
boundary. It is separate from `cloud/aws/foundation/template.yaml`, which is at the
CloudFormation inline template-size limit.

Current contents include the logical record schemas, a separate CloudFormation
template and a packaged Python handler. The template creates the protected
administrator group but does not add any user to it.

Schemas:

- `schemas/claim-record.schema.json`
- `schemas/vehicle-ownership-record.schema.json`
- `schemas/audit-event.schema.json`
- `schemas/lifecycle-operation-record.schema.json`

Create the deterministic local ZIP with:

```sh
tools/aws/package_onboarding_lambda.sh
```

Packaging does not upload or deploy the artifact. A later reviewed deployment must
upload it to a controlled artifact bucket and supply the foundation outputs, exact
CORS origin and artifact key as stack parameters. No API route, table, group, claim
or secret has been created in AWS by the local implementation.

Review [the claim data model](../../../docs/architecture/onboarding-claim-data-model.md)
and [the lifecycle design](../../../docs/architecture/onboarding-device-lifecycle.md)
before deployment or lifecycle extension.
