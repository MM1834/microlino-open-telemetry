#!/bin/zsh
set -euo pipefail

STACK_NAME="${STACK_NAME:-mot-aws-3-1}"
AWS_REGION="${AWS_REGION:-${AWS_DEFAULT_REGION:-}}"

aws_args=()
if [[ -n "$AWS_REGION" ]]; then
  aws_args+=(--region "$AWS_REGION")
fi

echo "==> Validate CloudFormation template"
aws cloudformation validate-template \
  "${aws_args[@]}" \
  --template-body file://template.yaml >/dev/null

echo "==> Deploy $STACK_NAME"
aws cloudformation deploy \
  "${aws_args[@]}" \
  --stack-name "$STACK_NAME" \
  --template-file template.yaml \
  --capabilities CAPABILITY_NAMED_IAM \
  --no-fail-on-empty-changeset

API_URL="$(aws cloudformation describe-stacks \
  "${aws_args[@]}" \
  --stack-name "$STACK_NAME" \
  --query "Stacks[0].Outputs[?OutputKey=='VehicleApiBaseUrl'].OutputValue | [0]" \
  --output text)"

LOG_GROUP="$(aws cloudformation describe-stacks \
  "${aws_args[@]}" \
  --stack-name "$STACK_NAME" \
  --query "Stacks[0].Outputs[?OutputKey=='VehicleApiLogGroupName'].OutputValue | [0]" \
  --output text)"

echo "==> API: $API_URL"
echo "==> Health (expected HTTP 200)"
curl --silent --show-error --fail-with-body \
  --write-out '\nHTTP %{http_code}\n' \
  "$API_URL/health"

echo "==> Protected route without token (expected HTTP 401)"
curl --silent --show-error \
  --output /tmp/mot-no-token-response.json \
  --write-out 'HTTP %{http_code}\n' \
  "$API_URL/api/vehicles"
cat /tmp/mot-no-token-response.json 2>/dev/null || true
print

if [[ -z "${TOKEN:-}" ]]; then
  print "TOKEN is not set; authenticated tests were skipped."
  print "Set the Cognito ID token in the current zsh session:"
  print "  export TOKEN='eyJ...'"
  print "Then rerun this script."
  exit 0
fi

echo "==> Protected route with token (expected HTTP 200)"
curl --silent --show-error --fail-with-body \
  --header "Authorization: Bearer $TOKEN" \
  --write-out '\nHTTP %{http_code}\n' \
  "$API_URL/api/vehicles"

echo "==> Recent authentication log entries"
aws logs filter-log-events \
  "${aws_args[@]}" \
  --log-group-name "$LOG_GROUP" \
  --filter-pattern '"authenticated_request"' \
  --start-time "$(( ($(date +%s) - 600) * 1000 ))" \
  --query 'events[].message' \
  --output text
