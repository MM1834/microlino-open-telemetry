# CACHE-001 isolated AWS test lane

This stack accepts SOC/Speed History backfill only on
`mot-test/<testVehicleId>/history/backfill/v1`. It has no live State, WebSocket,
portal or Cognito resources and cannot receive operational `mot/#` traffic.

The default stack is intentionally small: one on-demand DynamoDB table with a
three-day TTL, one 128-MiB Lambda, one exact IoT Rule, one test Thing/policy and
short-retention logs. The account's low Lambda quota does not permit reserved
concurrency without reducing the required unreserved pool below ten; exact topic,
batch and payload limits therefore form the request-volume boundary.

## Deployment boundary

Deployment is a separate operator action. Package `handler.py` as
`cache-test-backfill.zip`, upload it to an explicit versioned private S3 object
and pass that bucket/key through `ArtifactBucket` and `ArtifactKey`. Always review
the generated Change Set before execution.

Do not change the topic root to `mot`: the operational `mot/#` rule would also
receive the test messages. Do not attach a productive certificate. Create a new
ACTIVE certificate for the dedicated no-GPS B025 nanoESP32-C6-N16 test module, retain its private
key only in the ignored local credential workspace, and pass its certificate ARN
through `TestCertificateArn`.

AWS Budgets is account-level and is therefore created separately after confirming
the desired notification destination. Stack tags and resource names provide the
cost-allocation boundary; TTL, log retention, exact topic matching and reserved
Lambda concurrency provide technical volume limits.
