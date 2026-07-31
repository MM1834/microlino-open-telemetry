# Cognito User Pool – administrator guide

## Current capability

After SPR-0004B.1.1, the user directory exists, but there is no dashboard app client or login page. Do not create production users yet. A test user may be created only after SPR-0004B.1.2 provides an app client and the authentication flow can be tested.

## Verify the pool

Read the pool ID from CloudFormation outputs, then inspect it:

```bash
aws cognito-idp describe-user-pool \
  --user-pool-id <COGNITO_USER_POOL_ID> \
  --query 'UserPool.{Name:Name,Status:Status,DeletionProtection:DeletionProtection,MfaConfiguration:MfaConfiguration}' \
  --output table
```

Expected MFA configuration: `OFF`.

## Responsibility boundary

Cognito manages user identities and credentials. Vehicle ownership and fleet permissions are not stored in Cognito and will be implemented in `UserVehicleAccess` later.
