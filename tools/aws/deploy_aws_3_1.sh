#!/usr/bin/env bash
set -euo pipefail

REGION="${AWS_REGION:-eu-north-1}"
STACK_NAME="${MOT_AWS_STACK:-mot-aws-3-1}"
PROJECT_NAME="${MOT_PROJECT_NAME:-mot}"
ENVIRONMENT="${MOT_ENVIRONMENT:-dev}"
TOPIC_FILTER="${MOT_TOPIC_FILTER:-mot/#}"
LOG_RETENTION_DAYS="${MOT_LOG_RETENTION_DAYS:-7}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEMPLATE="$ROOT/cloud/aws/foundation/template.yaml"

aws sts get-caller-identity >/dev/null

echo "Deploying AWS-3.1 to $REGION ($STACK_NAME)"
aws cloudformation validate-template \
  --region "$REGION" \
  --template-body "file://$TEMPLATE" \
  >/dev/null

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
  --tags project="$PROJECT_NAME" environment="$ENVIRONMENT" component=aws-3-1

aws cloudformation describe-stacks \
  --region "$REGION" \
  --stack-name "$STACK_NAME" \
  --query 'Stacks[0].Outputs' \
  --output table
