# AWS-3.2 — Multi-vehicle current-state store

AWS-3.2 updates the existing `mot-aws-3-1` stack:

```text
MOT devices -> AWS IoT Core -> IoT Rule -> Lambda -> DynamoDB
```

Table:

```text
mot-dev-vehicle-state
PK: vehicleId
SK: topicSuffix
```

Only the latest value per vehicle/topic is kept. Older out-of-order messages cannot overwrite newer state.

Deploy:

```bash
./tools/aws/deploy_aws_3_2.sh
```

Inspect one vehicle:

```bash
./tools/aws/get_aws_3_2_vehicle_state.sh pioneer | jq
```

Verbose per-message application logs are disabled by default. Lambda still runs once per selected MQTT message, because every topic updates an independent state item. This reduces CloudWatch log volume, not the IoT Rule/Lambda invocation count. A later firmware batching step can reduce invocations further.

Enable temporary verbose logs:

```bash
MOT_VERBOSE_MESSAGE_LOGS=true ./tools/aws/deploy_aws_3_2.sh
./tools/aws/tail_aws_3_2_logs.sh
```
