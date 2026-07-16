#!/usr/bin/env bash
set -euo pipefail
REGION="${AWS_REGION:-eu-north-1}"
STACK_NAME="${MOT_AWS_STACK:-mot-aws-3-1}"
aws cloudformation delete-stack --region "$REGION" --stack-name "$STACK_NAME"
aws cloudformation wait stack-delete-complete --region "$REGION" --stack-name "$STACK_NAME"
echo "Deleted $STACK_NAME"
