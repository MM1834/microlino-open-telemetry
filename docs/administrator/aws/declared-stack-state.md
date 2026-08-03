# Declared AWS Stack State

> **Status:** Confirmed in repository configuration; deployment unverified
>
> **Audience:** Administrator, maintainer and security reviewer
>
> **Source of truth:** `cloud/aws/foundation/template.yaml`

## Evidence boundary

This page describes the current CloudFormation template. DOC-001 has not queried
AWS and does not claim that these resources, defaults or parameter values are
deployed. Historical stack names and validation checklists are supporting evidence
only.

## Declared resources

| Area | Resources declared by the template |
|---|---|
| Identity | Cognito User Pool, public dashboard client and managed-login domain |
| State | DynamoDB vehicle-state and live-connection tables |
| Ingestion | AWS IoT Topic Rule, Lambda and invoke permission |
| REST | HTTP API, JWT authorizer, Lambda, routes, stage and invoke permission |
| Live | WebSocket API, custom authorizer Lambda, handler Lambda, routes and stage |
| Access | Three named Lambda execution roles with inline policies |
| Logging | Explicit CloudWatch Log Groups for four Lambdas |

The Lambda application code is embedded inline in the CloudFormation template.

## Parameters and defaults

| Parameter | Template default | Interpretation |
|---|---|---|
| `ProjectName` | `mot` | Naming and tags |
| `Environment` | `dev` | Naming and tags |
| `TopicFilter` | `mot/#` | IoT Rule input namespace |
| `LogRetentionDays` | `7` | State-ingestion Lambda log retention |
| `EnableVerboseMessageLogs` | `false` | State-ingestion message-detail logging |
| `ApiAllowedOrigin` | `*` | HTTP API CORS origin |
| `ApiLogRetentionDays` | `7` | API/live Lambda log retention |
| `CognitoSelfRegistrationEnabled` | `false` | Admin-created users only by default |
| `CognitoDeletionProtection` | `ACTIVE` | User Pool deletion protection |
| callback/logout URLs | localhost | Development defaults, not deployed HTTPS proof |

Parameter defaults are not evidence of deployed parameter values.

## CORS and API stages

The HTTP API declares:

- origin from `ApiAllowedOrigin`, default `*`;
- methods `GET` and `OPTIONS`;
- headers `content-type` and `authorization`;
- preflight cache maximum of 300 seconds.

The REST and WebSocket stages use `$default` with automatic deployment. REST
throttling is declared as burst 20/rate 10; WebSocket throttling as burst 50/rate
25. Neither stage declares API Gateway access-log settings in the template.

The `ApiAllowedOrigin` description still calls the API unauthenticated and says
authentication is added later. That description is stale: list and snapshot routes
now use a JWT authorizer.

## Cognito

The template declares:

- email as username and auto-verified attribute;
- email-based account recovery;
- public self-registration disabled by default;
- MFA set to `OFF`;
- minimum password length 12 with uppercase, lowercase, number and symbol;
- temporary password validity of seven days;
- Authorization Code OAuth flow with `openid`, `email` and `profile` scopes;
- no browser client secret;
- access and ID token validity of 60 minutes;
- refresh token validity of 30 days;
- token revocation and user-existence-error protection enabled.

The dashboard currently does not use its refresh token and requires a new login
after access-token expiry.

## Data protection and retention

Both DynamoDB tables declare server-side encryption. The live-connection table
declares TTL on `expiresAt`. The template does not declare point-in-time recovery,
backup policy, deletion protection for DynamoDB or a telemetry-history store.

Explicit Lambda log retention is parameterized. `EnableVerboseMessageLogs=false`
reduces state-ingestion detail, but actual deployed logging and API Gateway access
logs remain unverified.

## IAM as declared

### State ingestion role

- `UpdateItem` on vehicle state;
- `Query` and `DeleteItem` on live connections and its vehicle index;
- `execute-api:ManageConnections` for the WebSocket API;
- AWS managed basic Lambda logging policy.

### Vehicle API role

- `Query`, `Scan` and `GetItem` on the complete vehicle-state table;
- AWS managed basic Lambda logging policy.

This matches the current all-vehicle discovery implementation and cannot enforce
per-user vehicle access.

### Live WebSocket role

- `PutItem`, `UpdateItem`, `DeleteItem` and `GetItem` on live connections;
- `execute-api:ManageConnections` on the WebSocket API;
- AWS managed basic Lambda logging policy.

The same role is used by the live authorizer and live handler. The authorizer thus
receives connection-table and connection-management permissions it does not appear
to require. This is a least-privilege review item, not proof of exploitability.

## Authentication and authorization boundary

- REST list and snapshot routes require a Cognito JWT.
- WebSocket `$connect` requires a custom authorizer that validates a Cognito access
  token.
- The WebSocket token is supplied as `access_token` in the query string.
- No user-to-vehicle access table or ownership check is declared.
- Authenticated users can currently request or subscribe to arbitrary vehicle IDs.

## Declared outputs

The template exports Cognito identifiers/endpoints, Region, IoT Rule and ingestion
names, vehicle-state table identifiers, Vehicle API ID/base URL/log group, WebSocket
API ID/URL, connection table and live Lambda names.

Outputs such as client ID, API URL and Cognito endpoint are configuration, not
private device credentials. Access tokens and device private keys remain secrets.

## Related documents

- [Read-only verification](read-only-verification.md)
- [Cloud risk register](../../security/cloud-risk-register.md)
- [Vehicle REST API](../../api/vehicle-api.md)
- [Live WebSocket API](../../api/live-websocket-api.md)
