#!/usr/bin/env bash
set -euo pipefail
REGION="${AWS_REGION:-eu-north-1}"
STACK_NAME="${MOT_AWS_STACK:-mot-aws-3-1}"
SINCE="${1:-10m}"
FUNCTION_NAME="$(aws cloudformation describe-stacks --region "$REGION" --stack-name "$STACK_NAME" --query "Stacks[0].Outputs[?OutputKey=='StateIngestFunctionName'].OutputValue" --output text)"
aws logs tail "/aws/lambda/$FUNCTION_NAME" --region "$REGION" --since "$SINCE" --follow --format short
