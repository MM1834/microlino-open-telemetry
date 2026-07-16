#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <region> <thing-name>"
  exit 2
fi

REGION="$1"
THING_NAME="$2"
DIR="private/aws/${THING_NAME}"
DEVICE_JSON="${DIR}/device.json"
POLICY_JSON="${DIR}/policy.json"

if [[ ! -f "$DEVICE_JSON" ]]; then
  echo "Missing $DEVICE_JSON"
  exit 1
fi

POLICY_NAME="$(jq -r '.policyName' "$DEVICE_JSON")"
ACCOUNT_ID="$(jq -r '.accountId' "$DEVICE_JSON")"
VEHICLE_ID="$(jq -r '.vehicleId' "$DEVICE_JSON")"

cat > "$POLICY_JSON" <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:${REGION}:${ACCOUNT_ID}:client/${THING_NAME}"
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish",
        "iot:RetainPublish"
      ],
      "Resource": [
        "arn:aws:iot:${REGION}:${ACCOUNT_ID}:topic/mot/${VEHICLE_ID}/*"
      ]
    },
    {
      "Effect": "Allow",
      "Action": "iot:Subscribe",
      "Resource": [
        "arn:aws:iot:${REGION}:${ACCOUNT_ID}:topicfilter/mot/${VEHICLE_ID}/commands/*",
        "arn:aws:iot:${REGION}:${ACCOUNT_ID}:topicfilter/mot/${VEHICLE_ID}/configuration/*"
      ]
    },
    {
      "Effect": "Allow",
      "Action": "iot:Receive",
      "Resource": [
        "arn:aws:iot:${REGION}:${ACCOUNT_ID}:topic/mot/${VEHICLE_ID}/commands/*",
        "arn:aws:iot:${REGION}:${ACCOUNT_ID}:topic/mot/${VEHICLE_ID}/configuration/*"
      ]
    }
  ]
}
EOF

aws iot create-policy-version \
  --region "$REGION" \
  --policy-name "$POLICY_NAME" \
  --policy-document "file://$POLICY_JSON" \
  --set-as-default

echo "Policy updated: $POLICY_NAME"
