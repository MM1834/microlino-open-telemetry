# AWS Onboarding Boundary

> **Status:** Planned design — no onboarding stack is deployed

This directory owns the future ONB-001.B2 packaged backend boundary. It is separate
from `cloud/aws/foundation/template.yaml`, which is at the CloudFormation inline
template-size limit.

Current contents are logical record schemas only:

- `schemas/claim-record.schema.json`
- `schemas/vehicle-ownership-record.schema.json`
- `schemas/audit-event.schema.json`
- `schemas/lifecycle-operation-record.schema.json`

No template, Lambda package, API route, table, claim or secret is created by these
files. See [the claim data model](../../../docs/architecture/onboarding-claim-data-model.md)
before adding implementation.
