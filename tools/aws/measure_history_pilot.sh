#!/usr/bin/env bash
set -euo pipefail

region="${AWS_REGION:-eu-north-1}"
project="${PROJECT_NAME:-mot}"
environment="${ENVIRONMENT:-dev}"
days="${DAYS:-7}"
end_time="$(date -u +%Y-%m-%d)T00:00:00Z"
start_time="$(date -u -v-"${days}"d +%Y-%m-%dT00:00:00Z 2>/dev/null || date -u -d "${days} days ago" +%Y-%m-%dT00:00:00Z)"
ingest_function="${project}-${environment}-state-ingest"
history_table="${project}-${environment}-vehicle-history"

echo "State-ingest invocations (one invocation per accepted mot/# rule action):"
aws cloudwatch get-metric-statistics \
  --region "${region}" \
  --namespace AWS/Lambda \
  --metric-name Invocations \
  --dimensions Name=FunctionName,Value="${ingest_function}" \
  --start-time "${start_time}" \
  --end-time "${end_time}" \
  --period 86400 \
  --statistics Sum \
  --output table

echo "History table writes (pay-per-request cost driver):"
aws cloudwatch get-metric-statistics \
  --region "${region}" \
  --namespace AWS/DynamoDB \
  --metric-name ConsumedWriteCapacityUnits \
  --dimensions Name=TableName,Value="${history_table}" \
  --start-time "${start_time}" \
  --end-time "${end_time}" \
  --period 86400 \
  --statistics Sum \
  --output table

echo "History API/table reads (portal cost driver):"
aws cloudwatch get-metric-statistics \
  --region "${region}" \
  --namespace AWS/DynamoDB \
  --metric-name ConsumedReadCapacityUnits \
  --dimensions Name=TableName,Value="${history_table}" \
  --start-time "${start_time}" \
  --end-time "${end_time}" \
  --period 86400 \
  --statistics Sum \
  --output table
