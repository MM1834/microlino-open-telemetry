#!/usr/bin/env bash
set -euo pipefail
REGION="${AWS_REGION:-eu-north-1}"
STACK_NAME="${MOT_AWS_STACK:-mot-aws-3-1}"
PROJECT_NAME="${MOT_PROJECT_NAME:-mot}"
ENVIRONMENT="${MOT_ENVIRONMENT:-dev}"
TOPIC_FILTER="${MOT_TOPIC_FILTER:-mot/#}"
LOG_RETENTION_DAYS="${MOT_LOG_RETENTION_DAYS:-7}"
VERBOSE_MESSAGE_LOGS="${MOT_VERBOSE_MESSAGE_LOGS:-false}"
API_ALLOWED_ORIGIN="${MOT_API_ALLOWED_ORIGIN:-*}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEMPLATE="$ROOT/cloud/aws/foundation/template.yaml"
aws cloudformation deploy \
  --region "$REGION" \
  --stack-name "$STACK_NAME" \
  --template-file "$TEMPLATE" \
  --capabilities CAPABILITY_NAMED_IAM \
  --no-fail-on-empty-changeset \
  --parameter-overrides \
    ProjectName="$PROJECT_NAME" \
    Environment="$ENVIRONMENT" \
    TopicFilter="$TOPIC_FILTER" \
    LogRetentionDays="$LOG_RETENTION_DAYS" \
    EnableVerboseMessageLogs="$VERBOSE_MESSAGE_LOGS" \
    ApiAllowedOrigin="$API_ALLOWED_ORIGIN" \
  --tags project="$PROJECT_NAME" environment="$ENVIRONMENT" component=aws-3-3
aws cloudformation describe-stacks --region "$REGION" --stack-name "$STACK_NAME" --query 'Stacks[0].Outputs' --output table
