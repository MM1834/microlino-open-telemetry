# User administration

## Status

SPR-0004B.1 supports administrator-created users. There is no fleet-admin web page yet. Until that interface exists, a trusted AWS administrator performs the initial lifecycle actions in the Cognito console or AWS CLI.

## Create the first test user

Use a real mailbox that can receive the temporary-password message.

```bash
aws cognito-idp admin-create-user \
  --user-pool-id <CognitoUserPoolId> \
  --username user@example.com \
  --user-attributes Name=email,Value=user@example.com Name=email_verified,Value=true
```

Do not paste temporary passwords into tickets, source files, chat logs or documentation.

## Disable a user

```bash
aws cognito-idp admin-disable-user \
  --user-pool-id <CognitoUserPoolId> \
  --username user@example.com
```

Disabling a Cognito user prevents future authentication. Vehicle access records will be introduced separately and must later be removed or audited as part of offboarding.

## Responsibility boundary

Cognito manages the account and login identity. It does not decide which vehicles the account may access. Vehicle assignments and the `owner`, `viewer` and `admin` authorization model are implemented in later SPR-0004 increments.
