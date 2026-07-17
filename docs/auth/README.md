# Authentication documentation

This section separates documentation by audience so implementation details and operational instructions do not get mixed with end-user guidance.

- [Developer documentation](developer/cognito-infrastructure.md)
- [Administrator documentation](admin/user-management.md)
- [End-user documentation](end-user/account-login.md)

## Current delivery status

SPR-0004B.1 creates the Cognito user pool and public dashboard app client. It does not yet add API authorization or a dashboard login screen.

| Audience | Available in SPR-0004B.1 | Deferred |
|---|---|---|
| Developer | Infrastructure, deployment, outputs and validation | JWT authorizer and dashboard integration |
| Fleet administrator | Manual user creation and lifecycle basics | Vehicle assignment and invitations |
| End user | Account lifecycle preview | Login, logout, reset and verification UI |
