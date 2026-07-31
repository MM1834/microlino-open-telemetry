#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  ./tools/bootstrap_aws_iot_thing.sh <region> <thing-name> <vehicle-id>

Example:
  ./tools/bootstrap_aws_iot_thing.sh eu-central-1 mot-esp32-f924f0 pioneer

Requirements:
  - AWS CLI v2 configured with an IAM identity allowed to manage AWS IoT
  - jq
  - curl

Creates:
  - AWS IoT Thing
  - active X.509 certificate/private key
  - least-privilege per-device IoT policy
  - exclusive Thing/certificate attachment
  - local credential directory under secrets/aws-iot/<thing-name>/

Important:
  The private key is created only once. Keep it secure.
EOF
}

if [[ $# -ne 3 ]]; then
  usage
  exit 2
fi

REGION="$1"
THING_NAME="$2"
VEHICLE_ID="$3"
POLICY_NAME="${THING_NAME}-policy"
OUT_DIR="secrets/aws-iot/${THING_NAME}"

command -v aws >/dev/null || { echo "aws CLI not found"; exit 1; }
command -v jq >/dev/null || { echo "jq not found"; exit 1; }
command -v curl >/dev/null || { echo "curl not found"; exit 1; }

if [[ ! "$THING_NAME" =~ ^[A-Za-z0-9:_-]+$ ]]; then
  echo "Invalid Thing name: $THING_NAME"
  exit 1
fi

if [[ ! "$VEHICLE_ID" =~ ^[A-Za-z0-9_-]+$ ]]; then
  echo "Invalid vehicle ID: $VEHICLE_ID"
  exit 1
fi

mkdir -p "$OUT_DIR"
chmod 700 "$OUT_DIR"

ACCOUNT_ID="$(aws sts get-caller-identity --query Account --output text)"
ENDPOINT="$(aws iot describe-endpoint \
  --region "$REGION" \
  --endpoint-type iot:Data-ATS \
  --query endpointAddress \
  --output text)"

echo "AWS account : $ACCOUNT_ID"
echo "AWS region  : $REGION"
echo "Thing       : $THING_NAME"
echo "Vehicle ID  : $VEHICLE_ID"
echo "IoT endpoint: $ENDPOINT"

aws iot create-thing \
  --region "$REGION" \
  --thing-name "$THING_NAME" \
  --attribute-payload "attributes={vehicleId=${VEHICLE_ID},boardType=esp32-wroom}" \
  >/dev/null

CERT_JSON="$OUT_DIR/certificate.json"

if [[ -e "$OUT_DIR/device-private-key.pem.key" ]]; then
  echo "Refusing to overwrite existing private key: $OUT_DIR/device-private-key.pem.key"
  exit 1
fi

aws iot create-keys-and-certificate \
  --region "$REGION" \
  --set-as-active \
  --certificate-pem-outfile "$OUT_DIR/device-certificate.pem.crt" \
  --public-key-outfile "$OUT_DIR/device-public-key.pem.key" \
  --private-key-outfile "$OUT_DIR/device-private-key.pem.key" \
  > "$CERT_JSON"

chmod 600 "$OUT_DIR/device-private-key.pem.key"
chmod 644 "$OUT_DIR/device-certificate.pem.crt" "$OUT_DIR/device-public-key.pem.key"

CERT_ARN="$(jq -r '.certificateArn' "$CERT_JSON")"
CERT_ID="$(jq -r '.certificateId' "$CERT_JSON")"

cat > "$OUT_DIR/policy.json" <<EOF
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

aws iot create-policy \
  --region "$REGION" \
  --policy-name "$POLICY_NAME" \
  --policy-document "file://$OUT_DIR/policy.json" \
  >/dev/null

aws iot attach-policy \
  --region "$REGION" \
  --policy-name "$POLICY_NAME" \
  --target "$CERT_ARN"

aws iot attach-thing-principal \
  --region "$REGION" \
  --thing-name "$THING_NAME" \
  --principal "$CERT_ARN" \
  --thing-principal-type EXCLUSIVE_THING

curl --fail --silent --show-error \
  https://www.amazontrust.com/repository/AmazonRootCA1.pem \
  -o "$OUT_DIR/AmazonRootCA1.pem"

cat > "$OUT_DIR/device.json" <<EOF
{
  "region": "${REGION}",
  "accountId": "${ACCOUNT_ID}",
  "endpoint": "${ENDPOINT}",
  "port": 8883,
  "thingName": "${THING_NAME}",
  "mqttClientId": "${THING_NAME}",
  "vehicleId": "${VEHICLE_ID}",
  "topicPrefix": "mot",
  "certificateId": "${CERT_ID}",
  "policyName": "${POLICY_NAME}"
}
EOF

cat <<EOF

Created successfully.

Credential directory:
  $OUT_DIR

AWS IoT MQTT test-client subscription:
  mot/${VEHICLE_ID}/#

Device settings:
  endpoint : $ENDPOINT
  port     : 8883
  clientId : $THING_NAME

Next:
  1. Back up this credential directory securely.
  2. Do not commit it.
  3. Validate the certificate with a desktop MQTT client.
  4. Then integrate MQTT/TLS into ESP32-WROOM.
EOF
