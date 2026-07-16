#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <thing-name> [vehicle-id]"
  exit 2
fi

THING="$1"
VEHICLE="${2:-pioneer}"
DIR="private/aws/$THING"
ENDPOINT="$(jq -r .endpoint "$DIR/device.json")"
UTC="$(date -u +%s)"
TOPIC="mot/$VEHICLE/system/aws_3_1_test"
PAYLOAD="{\"ok\":true,\"utc\":$UTC,\"source\":\"desktop\"}"

mosquitto_pub \
  --host "$ENDPOINT" \
  --port 8883 \
  --cafile "$DIR/AmazonRootCA1.pem" \
  --cert "$DIR/device-certificate.pem.crt" \
  --key "$DIR/device-private-key.pem.key" \
  --id "$THING" \
  --topic "$TOPIC" \
  --message "$PAYLOAD" \
  --qos 1 \
  --debug
