# Dashboard app client operations

## Administrator scope

The app client identifies the Microlino dashboard to Cognito. Administrators normally manage its allowed callback and logout URLs through CloudFormation rather than editing the Cognito console manually.

## Adding an environment URL

Use exact URLs. Production and shared test environments must use HTTPS.

Example parameter values:

```text
CognitoDashboardCallbackUrls=https://dashboard.example.org/callback
CognitoDashboardLogoutUrls=https://dashboard.example.org/
```

Multiple URLs can be supplied as comma-separated values. Review them carefully because an overly broad or incorrect redirect URL weakens the authentication boundary.

## Operational rules

- Do not create a client secret for the browser dashboard.
- Do not enable the implicit OAuth flow.
- Do not add wildcard callback URLs; Cognito requires exact URLs.
- Apply changes through a reviewed CloudFormation change set.
- Record environment URL changes in the project changelog.

## Current limitation

SPR-0004B.1.2 creates the app client only. A Cognito domain is added in the next increment, so interactive managed login is not available yet.
