#!/usr/bin/env bash
set -euo pipefail

VEHICLE_ID="${1:-pioneer}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
API_URL="$("$ROOT/tools/aws/get_aws_3_3_api_url.sh")"

echo "API: $API_URL"
echo
echo "Health:"
curl --fail --silent --show-error \
  "$API_URL/health" | jq

echo
echo "Vehicles:"
curl --fail --silent --show-error \
  "$API_URL/api/vehicles" | jq

echo
echo "Snapshot for $VEHICLE_ID:"
curl --fail --silent --show-error \
  "$API_URL/api/vehicles/$VEHICLE_ID/snapshot" | jq
