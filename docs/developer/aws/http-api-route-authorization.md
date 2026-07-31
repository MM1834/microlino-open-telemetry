# HTTP API route authorization

`VehicleApiJwtAuthorizer` validates Cognito JWTs. Protected routes use `AuthorizationType: JWT` and `AuthorizerId: !Ref VehicleApiJwtAuthorizer`. API Gateway rejects invalid tokens before Lambda.
