# Cognito infrastructure

## Purpose

Amazon Cognito authenticates dashboard users. AWS IoT Core continues to authenticate firmware devices with X.509 certificates. The two identity systems are intentionally independent.

SPR-0004B.1 adds these CloudFormation resources to `cloud/aws/foundation/template.yaml`:

- `DashboardUserPool`
- `DashboardUserPoolClient`

The browser app client has no client secret. A secret cannot be protected in browser-delivered JavaScript.

## Defaults

- Sign-in identifier: email address
- Email verification: enabled
- Public self-registration: disabled
- MFA: disabled for the first increment
- Minimum password length: 12 characters
- Access and ID token validity: 60 minutes
- Refresh token validity: 30 days
- Token revocation: enabled
- User-pool deletion protection: active

Public registration remains disabled until onboarding, abuse prevention, legal text and support processes exist.

## Validate before deployment

```bash
aws cloudformation validate-template \
  --template-body file://cloud/aws/foundation/template.yaml
```

## Deploy

Use the same stack name and region as the existing foundation deployment. Review the change set before execution.

```bash
aws cloudformation deploy \
  --template-file cloud/aws/foundation/template.yaml \
  --stack-name mot-dev-foundation \
  --parameter-overrides \
    ProjectName=mot \
    Environment=dev \
    CognitoSelfRegistrationEnabled=false \
    CognitoDeletionProtection=ACTIVE \
  --capabilities CAPABILITY_NAMED_IAM
```

Existing parameter overrides such as CORS origin and log retention must be retained when they differ from defaults.

## Read outputs

```bash
aws cloudformation describe-stacks \
  --stack-name mot-dev-foundation \
  --query 'Stacks[0].Outputs[?starts_with(OutputKey, `Cognito`)]' \
  --output table
```

The app client ID and user pool ID are configuration values, not credentials. Passwords, access tokens, refresh tokens and temporary passwords must never be committed or logged.

## Rollback warning

Deletion protection intentionally prevents accidental removal of the user pool. To delete a development pool, first update `CognitoDeletionProtection=INACTIVE`, confirm that no required users remain, and only then remove the resource.

## Next increment

SPR-0004B.2 will use `CognitoIssuerUrl` and `CognitoUserPoolClientId` to configure an API Gateway HTTP API JWT authorizer. Protected routes will receive verified claims under `requestContext.authorizer.jwt.claims`.
