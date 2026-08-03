# SPR-0004B.2.2 – Protected vehicle routes

This increment enables the existing Cognito JWT authorizer on the vehicle list and snapshot routes. The health route remains public.

The package uses a guarded, idempotent update script so the current CloudFormation template is preserved exactly.

See `DEPLOYMENT.md`.
