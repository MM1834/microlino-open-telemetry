#!/usr/bin/env bash
set -euo pipefail
VEHICLE_ID="${1:-pioneer}"
REGION="${AWS_REGION:-eu-north-1}"
STACK_NAME="${MOT_AWS_STACK:-mot-aws-3-1}"
TABLE_NAME="$(aws cloudformation describe-stacks --region "$REGION" --stack-name "$STACK_NAME" --query "Stacks[0].Outputs[?OutputKey=='VehicleStateTableName'].OutputValue" --output text)"
aws dynamodb query --region "$REGION" --table-name "$TABLE_NAME" --key-condition-expression 'vehicleId = :v' --expression-attribute-values "{\":v\":{\"S\":\"$VEHICLE_ID\"}}" --consistent-read --output json
