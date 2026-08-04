# Cloud Risk and Gap Register

> **Status:** Current code and development-deployment findings
>
> **Audience:** Maintainer, security reviewer and onboarding architect

This register identifies gaps visible in repository code and configuration. It is
not a penetration test and does not assert that the current template is deployed.

| ID | Finding | Evidence | Consequence | Required next decision |
|---|---|---|---|---|
| CLOUD-001 | Multi-user isolation was not proven end to end | Resolved in controlled development 2026-08-03 and repeated through the production `/dashboard/` portal on 2026-08-04 for both users and all three devices | Original cross-user disclosure blocker is removed for the tested stack | Retain regression gates for future authorization changes |
| CLOUD-002 | WebSocket access token in query string | `$connect` identity source is `access_token`; dashboard adds token to URL | Token may appear in intermediary/access logs | Review transport pattern and logging/redaction |
| CLOUD-003 | Production portal origin not yet defined | Resolved for the controlled portal on 2026-08-03: effective Cognito URLs and both API CORS configurations use the exact canonical `www` HTTPS host; `/dashboard/` production-path acceptance passed 2026-08-04 | Original undefined-origin blocker is removed; non-`www` still serves TLS content directly because available Domaincenter rules do not precede the Plesk vHost | Bounded pilot exception accepted 2026-08-03; publish only `www` links and revisit server-side canonicalization before broader public rollout |
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
| CLOUD-016 | Telemetry request amplification and billing visibility | CloudWatch recorded about 2.88 million daily-binned state-ingest invocations from 2026-07-15 through 2026-08-03. A narrower hourly query measured 1,761,194 invocations across 119 active hours with regular plateaus near 17,400/hour. Cost Explorer remains denied to the maintainer IAM user | Per-topic IoT Rule, Lambda, DynamoDB and live fan-out operations produce recurring cost and scale linearly with publish frequency | Keep HIS-001 bucketed and allowlisted, observe its 1,000-write daily alarm, obtain billing visibility/export and evaluate firmware batching before fleet growth |
| CLOUD-017 | OAuth callback query parameters in web access logs | Confirmed 2026-08-03 in both hosted `access_ssl_log` and `proxy_access_ssl_log` for `/motbeta/callback/`; no values were copied into project records. On 2026-08-04 the provider confirmed that individual log redaction or callback exclusion is unavailable on the shared-hosting service and would require a vServer subscription | One-time PKCE-bound authorization artifacts are duplicated in hosting logs even after successful exchange | Continue the bounded pilot acceptance; do not buy a vServer solely for the pilot portal. Evaluate migration of only the static portal to controlled AWS hosting before broader rollout |

## Priority boundary

CLOUD-001 and the original CLOUD-003 origin blocker are resolved for the controlled
portal. CLOUD-002, canonical host routing and pilot credential handling require
review before a general release. CLOUD-017 is accepted only for the bounded pilot
defined below. CLOUD-014/CLOUD-015 are locally resolved
but require review of the committed change before any credential upload. Other
findings may be accepted temporarily with an explicit rationale and bounded pilot
scope.

## CLOUD-017 bounded pilot acceptance

On 2026-08-03 the maintainer explicitly accepted CLOUD-017 for the controlled
REL-001 pilot. On 2026-08-04 the hosting provider answered that the requested
individual treatment of `access_ssl_log` and `proxy_access_ssl_log` cannot be
implemented on the shared-hosting service; individual configuration is available
only with a vServer subscription. The maintainer decided not to add that fixed
hosting cost solely for the small pilot portal.

The acceptance is limited to:

- directly invited and supported pilot accounts; no public self-registration;
- the `/dashboard/` pilot portal, the retained `/motbeta/` fallback and their
  Cognito Authorization Code + PKCE flow;
- no claim proof, token, password or device credential in application URLs;
- access to hosting logs restricted to the maintainer's hosting account;
- no copying of callback query values into tickets, screenshots, repository
  records or support bundles;
- the shortest practical hosting-log rotation and deletion available to the
  maintainer;
- renewed review before public self-registration, before materially expanding the
  pilot group, or before calling the portal a general public release.

The rationale is that the exposed authorization code is one-time, short-lived and
PKCE-bound, while account creation and pilot support remain controlled. This lowers
but does not remove the consequence of log access. The acceptance does not resolve
CLOUD-017 and must not be carried silently into a public rollout.

## CLOUD-017 preferred later remediation

The preferred low-fixed-cost option is to migrate only the static portal to an
AWS-controlled origin, while retaining the landing page and unrelated website
content on the existing shared hosting. The initial candidate is a private S3
origin behind CloudFront on a dedicated portal hostname, with CloudFront standard
access logging and S3 server access logging disabled unless a reviewed
privacy-safe logging design is introduced.

That migration is deferred while the directly supported pilot remains small. It
becomes a release decision before public self-registration, material pilot growth
or general-public positioning. Implementation must include:

- a dedicated portal hostname and TLS certificate;
- deployment and rollback of the static `dashboard/` package;
- exact Cognito callback and logout URL changes;
- exact REST and WebSocket origin/CORS review;
- confirmation that no upstream or origin access log retains OAuth callback query
  values;
- hosted login, logout, session-expiry, role and cross-user-isolation regression
  tests;
- a small AWS budget alert and post-deployment cost observation.

A callback-only edge redirect and a server-side API Gateway/Lambda callback remain
possible alternatives, but both add authentication-flow complexity. They are not
the preferred pilot remediation. Replacing Authorization Code + PKCE with an
implicit token flow is not an accepted alternative.

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
