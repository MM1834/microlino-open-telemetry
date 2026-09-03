# AWS Onboarding Boundary

> **Status:** Deployed controlled-beta onboarding and WEBFLASH-001 backend

AWS CloudFormation `validate-template` accepts `template.yaml` in `eu-north-1`.
The controlled development stack is deployed as `mot-dev-onboarding` and owns the
claim boundary plus the WEBFLASH-001 server foundation.

This directory owns the future ONB-001.B2 claim and B3 lifecycle packaged backend
boundary. It is separate from `cloud/aws/foundation/template.yaml`, which is at the
CloudFormation inline template-size limit.

Current contents include the logical record schemas, a separate CloudFormation
template and a packaged Python handler. The template creates the protected
administrator group but does not add any user to it. WEBFLASH-001 additionally
owns an encrypted TTL grant table, private encrypted/versioned firmware bucket,
admin grant/revoke routes, authenticated access/download/result routes and bounded
audit events. Firmware downloads use five-minute presigned S3 URLs; Lambda never
proxies the binary.

Schemas:

- `schemas/claim-record.schema.json`
- `schemas/vehicle-ownership-record.schema.json`
- `schemas/audit-event.schema.json`
- `schemas/lifecycle-operation-record.schema.json`

Create the deterministic local ZIP with:

```sh
tools/aws/package_onboarding_lambda.sh
```

Packaging does not upload or deploy the artifact by itself. Every update still
requires an immutable uploaded Lambda package, explicit stack parameters and a
reviewed no-replacement Change Set. Firmware releases are separately packaged by
`tools/package_n16_webflash_release.py` (N16 default or explicit
`--target xiao-esp32c6`), uploaded to the private stack bucket and activated only
through exact target, flash geometry, version, size, SHA-256 and object-key
parameters. The backend exposes an exact bounded release catalog; each user grant
selects one target, and conflicting simultaneous active grants fail closed.

Review [the claim data model](../../../docs/architecture/onboarding-claim-data-model.md)
and [the lifecycle design](../../../docs/architecture/onboarding-device-lifecycle.md)
before deployment or lifecycle extension.
