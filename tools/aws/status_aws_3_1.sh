#!/usr/bin/env bash
set -euo pipefail
REGION="${AWS_REGION:-eu-north-1}"
STACK_NAME="${MOT_AWS_STACK:-mot-aws-3-1}"
aws cloudformation describe-stacks \
  --region "$REGION" \
  --stack-name "$STACK_NAME" \
  --query 'Stacks[0].{Status:StackStatus,Outputs:Outputs}' \
  --output json
RULE_NAME="$(aws cloudformation describe-stacks \
  --region "$REGION" \
  --stack-name "$STACK_NAME" \
  --query "Stacks[0].Outputs[?OutputKey=='IoTRuleName'].OutputValue" \
  --output text)"
aws iot get-topic-rule \
  --region "$REGION" \
  --rule-name "$RULE_NAME" \
  --output json
