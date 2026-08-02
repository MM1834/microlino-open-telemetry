# Cloud Risk and Gap Register

> **Status:** Current code and development-deployment findings
>
> **Audience:** Maintainer, security reviewer and onboarding architect

This register identifies gaps visible in repository code and configuration. It is
not a penetration test and does not assert that the current template is deployed.

| ID | Finding | Evidence | Consequence | Required next decision |
|---|---|---|---|---|
| CLOUD-001 | Multi-user isolation not yet proven end to end | ONB-001.A is deployed with controlled assignments; local negative tests pass, but only one confirmed Cognito identity exists | A deployment defect could remain undiscovered before mutually untrusted beta access | Create a second controlled identity and execute REST/WebSocket cross-user tests |
| CLOUD-002 | WebSocket access token in query string | `$connect` identity source is `access_token`; dashboard adds token to URL | Token may appear in intermediary/access logs | Review transport pattern and logging/redaction |
| CLOUD-003 | Production portal origin not yet defined | Development CORS is deployed as exact `http://localhost:8080`; no hosted portal was found | A production deploy cannot be safely configured yet | Select hosting and supply its exact HTTPS origin/callback/logout URLs |
| CLOUD-004 | Deployed state inventory was missing | Read-only inventory and ONB-001.A development deployment were completed 2026-08-02 | Future drift can make evidence stale | Repeat inventory at release gates |
| CLOUD-005 | Shared live authorizer/handler IAM role | ONB-001.A deployed a separate log-only authorizer role | Original excess-permission finding is remediated in development | Verify effective IAM policy during post-deploy review |
| CLOUD-006 | API Gateway access logging not declared | Stage resources lack access-log settings | Limited auditability; token logging behaviour unknown | Define privacy-safe access-log policy |
| CLOUD-007 | No cloud history/retention model | DynamoDB stores latest topic state only | Portal history expectations may be misleading | Define later data lifecycle and privacy model |
| CLOUD-008 | No user/device claim lifecycle | No claim records, transfer, replacement or revocation APIs | Beta onboarding cannot safely bind people and devices | ONB-001 lifecycle design |
| CLOUD-009 | Temporary credential workflow | Ignored local folders and LittleFS staging | Local theft/misrouting risk; manual lifecycle burden | Beta provisioning controls, later protected workflow |
| CLOUD-010 | Inline backend code in one template | Lambda code embedded in CloudFormation | Testing, review and packaging become harder as onboarding grows | Decide extraction boundary during ONB-001 |
| CLOUD-011 | MFA disabled | Cognito template sets `MfaConfiguration: OFF` | Account protection relies on password/email recovery | Decide beta and production MFA policy |
| CLOUD-012 | Refresh flow unused | Dashboard stores but does not use refresh token | Re-login after expiry; retained token adds limited benefit | Decide session lifecycle before onboarding release |
| CLOUD-013 | DynamoDB recovery controls absent | No PITR/deletion protection declared | State loss/recovery behaviour undefined | Evaluate after data classification and beta needs |
| CLOUD-014 | WROOM credential staging was not ignored | Upload tool copies four files into `firmware/esp32-wroom/data/aws`; SPR-0005.A added the missing rule | Accidental commit risk mitigated for known staging names | Resolved locally; retain `git check-ignore` release check |
| CLOUD-015 | Shared credential uploader lacked WROOM assignment guards | Tool accepted mismatched `device.json.thingName` and arbitrary environment | Credentials could be uploaded to the wrong Thing/firmware target | Resolved locally with fail-closed validation and unit tests; no upload performed |

## Priority boundary

CLOUD-001 is the blocker for multiple mutually untrusted beta users. CLOUD-002,
CLOUD-003, CLOUD-004 and beta credential handling require review before a public or
shared portal release. CLOUD-014/CLOUD-015 are locally resolved but require review
of the committed change before any credential upload. Other findings may be
accepted temporarily with an explicit rationale and bounded beta scope.

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
