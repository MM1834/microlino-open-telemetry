# AWS-3.1 — Cloud Foundation

AWS-3.1 creates:

```text
MOT device
  -> AWS IoT Core
  -> IoT Rule (`mot/#`)
  -> diagnostic Lambda
  -> CloudWatch logs and metrics
```

No database, user model or Dashboard API is introduced yet.

## Deploy

```bash
./tools/aws/deploy_aws_3_1.sh
```

Defaults:

```text
Region: eu-north-1
Stack: mot-aws-3-1
Environment: dev
Topic filter: mot/#
Log retention: 7 days
```

## Observe traffic

```bash
./tools/aws/tail_aws_3_1_logs.sh
```

## Desktop test

```bash
./tools/aws/test_aws_3_1.sh mot-lilygo-fe8ce0 pioneer
```

## Inspect

```bash
./tools/aws/status_aws_3_1.sh
```

## Destroy

```bash
./tools/aws/destroy_aws_3_1.sh
```

The stack records actual topic names, payload types, payload sizes and message
frequency before AWS-3.2 selects a multi-vehicle state-store design.
