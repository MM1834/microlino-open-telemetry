#!/usr/bin/env bash
set -euo pipefail

OPS_REGION="${AWS_REGION:-eu-north-1}"
OPS_TOPIC_NAME="${MOT_OPERATIONS_TOPIC_NAME:-mot-dev-sms-operations}"
OPS_CONCURRENCY_THRESHOLD="${MOT_LAMBDA_CONCURRENCY_WARNING:-800}"

OPS_ACCOUNT_ID="$(aws sts get-caller-identity --query Account --output text)"
OPS_TOPIC_ARN="arn:aws:sns:${OPS_REGION}:${OPS_ACCOUNT_ID}:${OPS_TOPIC_NAME}"

aws sns get-topic-attributes \
  --region "$OPS_REGION" \
  --topic-arn "$OPS_TOPIC_ARN" \
  --query 'Attributes.TopicArn' \
  --output text >/dev/null

aws cloudwatch put-metric-alarm \
  --region "$OPS_REGION" \
  --alarm-name mot-dev-lambda-throttles \
  --alarm-description "OPS-001: any regional Lambda throttle can affect portal or telemetry" \
  --namespace AWS/Lambda \
  --metric-name Throttles \
  --statistic Sum \
  --period 60 \
  --evaluation-periods 1 \
  --datapoints-to-alarm 1 \
  --threshold 0 \
  --comparison-operator GreaterThanThreshold \
  --treat-missing-data notBreaching \
  --alarm-actions "$OPS_TOPIC_ARN"

aws cloudwatch put-metric-alarm \
  --region "$OPS_REGION" \
  --alarm-name mot-dev-lambda-concurrency-warning \
  --alarm-description "OPS-001: regional Lambda concurrency is near the effective pilot quota" \
  --namespace AWS/Lambda \
  --metric-name ConcurrentExecutions \
  --statistic Maximum \
  --period 60 \
  --evaluation-periods 2 \
  --datapoints-to-alarm 2 \
  --threshold "$OPS_CONCURRENCY_THRESHOLD" \
  --comparison-operator GreaterThanOrEqualToThreshold \
  --treat-missing-data notBreaching \
  --alarm-actions "$OPS_TOPIC_ARN"

aws cloudwatch describe-alarms \
  --region "$OPS_REGION" \
  --alarm-names mot-dev-lambda-throttles mot-dev-lambda-concurrency-warning \
  --query 'MetricAlarms[].{Name:AlarmName,State:StateValue,Metric:MetricName,Threshold:Threshold}' \
  --output table
