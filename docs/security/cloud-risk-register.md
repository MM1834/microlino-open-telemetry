# Cloud Risk and Gap Register

> **Status:** Current code and development-deployment findings
>
> **Audience:** Maintainer, security reviewer and onboarding architect

This register identifies gaps visible in repository code and configuration. It is
not a penetration test and does not assert that the current template is deployed.

| ID | Finding | Evidence | Consequence | Required next decision |
|---|---|---|---|---|
| CLOUD-001 | Multi-user isolation was not proven end to end | Resolved in controlled development 2026-08-03: two confirmed identities, exclusive lists, symmetric guessed-ID denial, live revoke/restore and expiry rejection passed | Original cross-user disclosure blocker is removed for the tested stack | Retain regression gates and repeat against the production portal configuration |
| CLOUD-002 | WebSocket access token in query string | `$connect` identity source is `access_token`; dashboard adds token to URL | Token may appear in intermediary/access logs | Review transport pattern and logging/redaction |
| CLOUD-003 | Production portal origin not yet defined | Resolved for the controlled portal on 2026-08-03: effective Cognito URLs and both API CORS configurations use the exact canonical `www` HTTPS host | Original undefined-origin blocker is removed; non-`www` currently serves content without redirecting | Configure a canonical non-`www` to `www` redirect or record a bounded pilot exception |
| CLOUD-004 | Deployed state can drift from stack parameters | Read-only inventory and ONB-001.A deployment completed 2026-08-02; later logout testing found app-client URL drift despite stale CloudFormation parameter values | Parameter inspection alone can falsely indicate a safe effective configuration | Compare effective resources with parameters at release gates; localhost and both retained portal URLs were reconciled 2026-08-02 |
| CLOUD-005 | Shared live authorizer/handler IAM role | ONB-001.A deployed a separate log-only authorizer role | Original excess-permission finding is remediated in development | Verify effective IAM policy during post-deploy review |
| CLOUD-006 | API Gateway access logging not declared | Stage resources lack access-log settings | Limited auditability; token logging behaviour unknown | Define privacy-safe access-log policy |
| CLOUD-007 | No cloud history/retention model | DynamoDB stores latest topic state only | Portal history expectations may be misleading | Define later data lifecycle and privacy model |
| CLOUD-008 | User/device claim lifecycle is incomplete | ONB-001.B2 claim issuance and atomic consumption are deployed and hosted-portal validated; B3 replacement/transfer remains design-only | New controlled claims work, but lifecycle recovery still requires maintainer handling | Keep B3 outside the first bounded release and implement it before claiming automated replacement/recovery support |
| CLOUD-009 | Temporary credential workflow | Ignored local folders and LittleFS staging | Local theft/misrouting risk; manual lifecycle burden | Beta provisioning controls, later protected workflow |
| CLOUD-010 | Inline backend code in one template | Lambda code embedded in CloudFormation | Testing, review and packaging become harder as onboarding grows | Decide extraction boundary during ONB-001 |
| CLOUD-011 | MFA disabled | Cognito template sets `MfaConfiguration: OFF` | Account protection relies on password/email recovery | Decide beta and production MFA policy |
| CLOUD-012 | Refresh flow unused | Dashboard stores but does not use refresh token | Re-login after expiry; retained token adds limited benefit | Decide session lifecycle before onboarding release |
| CLOUD-013 | DynamoDB recovery controls absent | No PITR/deletion protection declared | State loss/recovery behaviour undefined | Evaluate after data classification and beta needs |
| CLOUD-014 | WROOM credential staging was not ignored | Upload tool copies four files into `firmware/esp32-wroom/data/aws`; SPR-0005.A added the missing rule | Accidental commit risk mitigated for known staging names | Resolved locally; retain `git check-ignore` release check |
| CLOUD-015 | Shared credential uploader lacked WROOM assignment guards | Tool accepted mismatched `device.json.thingName` and arbitrary environment | Credentials could be uploaded to the wrong Thing/firmware target | Resolved locally with fail-closed validation and unit tests; no upload performed |
| CLOUD-016 | Telemetry request amplification and billing visibility | CloudWatch recorded about 2.78 million state-ingest Lambda invocations from 2026-07-01 through 2026-08-04; current IAM user cannot read Cost Explorer | Per-topic IoT Rule, Lambda, DynamoDB and live fan-out operations may produce low but non-zero recurring cost and scale linearly with publish frequency | Enable billing visibility/budget alert; measure per-device publishes and evaluate batching before fleet growth |
| CLOUD-017 | OAuth callback query parameters in web access logs | The standard local Python server logged the PKCE authorization code and state during ONB-001.B2 testing; codes were one-time and already consumed | Short-lived authentication artifacts may remain in terminal or hosting access logs | Use the redacting local server; verify production hosting strips callback query strings and apply bounded log retention |

## Priority boundary

CLOUD-001 and the original CLOUD-003 origin blocker are resolved for the controlled
portal. CLOUD-002, CLOUD-017, canonical host routing and pilot credential handling
require review before a general release. CLOUD-014/CLOUD-015 are locally resolved
but require review of the committed change before any credential upload. Other
findings may be accepted temporarily with an explicit rationale and bounded pilot
scope.

## Relationship to documentation

- An entry marked as a risk is not automatically an implementation requirement.
- Target decisions belong in ONB-001 or a dedicated ADR.
- Remediation is documented as complete only after implementation and validation.
- The ChatGPT Classic export may clarify historical intent but cannot substitute
  for current code or deployed-state verification.

## Related documents

- [Declared stack state](../administrator/aws/declared-stack-state.md)
- [Read-only verification](../administrator/aws/read-only-verification.md)
- [AWS architecture](../architecture/aws-iot.md)
- [DOC-001](../project/sprints/DOC-001.md)
