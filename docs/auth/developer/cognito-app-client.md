# Cognito dashboard app client

## Purpose

`DashboardUserPoolClient` represents the browser dashboard in Amazon Cognito. It is a **public OAuth client** and therefore has no client secret.

The client is configured for the OAuth 2.0 authorization-code grant. The dashboard must use PKCE when it exchanges an authorization code for tokens. PKCE is implemented in the dashboard application; it is not a separate CloudFormation switch.

## Security decisions

- `GenerateSecret: false`: browser applications cannot securely retain a client secret.
- Only the `code` OAuth flow is enabled; the implicit grant is not enabled.
- Scopes are limited to `openid`, `email`, and `profile`.
- Access and ID tokens expire after 60 minutes.
- Refresh tokens expire after 30 days.
- Token revocation is enabled.
- User-existence errors are suppressed.

The client ID is public configuration. Tokens, authorization codes, PKCE code verifiers, and user credentials must never be committed or logged.

## Redirect URLs

CloudFormation parameters:

- `CognitoDashboardCallbackUrls`
- `CognitoDashboardLogoutUrls`

The defaults target local development:

```text
http://localhost:8080/callback
http://localhost:8080/
```

Deployed environments must use exact HTTPS URLs. Cognito compares redirect URLs exactly, including scheme, host, port, path, and trailing slash.

## Verification

After deployment, retrieve the client ID from the stack output:

```bash
aws cloudformation describe-stacks \
  --stack-name mot-aws-3-1 \
  --query 'Stacks[0].Outputs[?OutputKey==`CognitoUserPoolClientId`].OutputValue | [0]' \
  --output text
```

Inspect the client:

```bash
USER_POOL_ID=$(aws cloudformation describe-stacks \
  --stack-name mot-aws-3-1 \
  --query 'Stacks[0].Outputs[?OutputKey==`CognitoUserPoolId`].OutputValue | [0]' \
  --output text)

CLIENT_ID=$(aws cloudformation describe-stacks \
  --stack-name mot-aws-3-1 \
  --query 'Stacks[0].Outputs[?OutputKey==`CognitoUserPoolClientId`].OutputValue | [0]' \
  --output text)

aws cognito-idp describe-user-pool-client \
  --user-pool-id "$USER_POOL_ID" \
  --client-id "$CLIENT_ID" \
  --query 'UserPoolClient.{ClientName:ClientName,GenerateSecret:GenerateSecret,AllowedOAuthFlows:AllowedOAuthFlows,AllowedOAuthScopes:AllowedOAuthScopes,CallbackURLs:CallbackURLs,LogoutURLs:LogoutURLs,EnableTokenRevocation:EnableTokenRevocation,PreventUserExistenceErrors:PreventUserExistenceErrors}' \
  --output yaml
```

## Not included in this increment

- Cognito domain or managed-login pages
- dashboard PKCE implementation
- API Gateway JWT authorizer
- user-to-vehicle authorization
