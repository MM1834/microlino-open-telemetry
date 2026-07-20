# Cognito managed-login domain

## Purpose

`DashboardUserPoolDomain` exposes the Cognito OAuth 2.0 endpoints required by the browser dashboard. It is attached to the existing `DashboardUserPool`; it does not replace or modify the user pool or its app client.

## CloudFormation resource

```yaml
DashboardUserPoolDomain:
  Type: AWS::Cognito::UserPoolDomain
  Properties:
    Domain: !Sub "${ProjectName}-${Environment}-${AWS::AccountId}-${AWS::Region}"
    UserPoolId: !Ref DashboardUserPool
```

The generated prefix includes account and region so deployments in different accounts or regions do not normally collide. Cognito domain prefixes must nevertheless be globally available when the stack creates the resource.

## Endpoints

The stack exports:

- `CognitoHostedUiBaseUrl`
- `CognitoAuthorizationEndpoint`
- `CognitoTokenEndpoint`
- `CognitoLogoutEndpoint`

The browser will later start an Authorization Code flow with PKCE at the authorization endpoint. The application exchanges the returned code at the token endpoint. No client secret is used.

## Scope of this increment

This increment creates the domain only. It does not:

- add API authorization;
- create users;
- implement PKCE in the dashboard;
- change callback or logout URLs;
- add a custom domain or certificate.
