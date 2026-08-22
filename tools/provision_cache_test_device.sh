#!/usr/bin/env bash
set -euo pipefail

REGION="${AWS_REGION:-eu-north-1}"
STACK_NAME="${1:-mot-cachetest}"
OUTPUT_ROOT="private/aws"

command -v aws >/dev/null || { echo "aws CLI not found"; exit 1; }
command -v curl >/dev/null || { echo "curl not found"; exit 1; }
command -v jq >/dev/null || { echo "jq not found"; exit 1; }

stack_output() {
  aws cloudformation describe-stacks \
    --region "$REGION" \
    --stack-name "$STACK_NAME" \
    --query "Stacks[0].Outputs[?OutputKey=='$1'].OutputValue | [0]" \
    --output text
}

THING_NAME="$(stack_output TestThingName)"
VEHICLE_ID="$(stack_output TestVehicleId)"
TOPIC_ROOT="$(stack_output TopicRoot)"
POLICY_NAME="$(stack_output TestPolicyName)"

if [[ -z "$THING_NAME" || "$THING_NAME" == "None" ||
      -z "$VEHICLE_ID" || "$VEHICLE_ID" == "None" ||
      "$TOPIC_ROOT" != "mot-test" ]]; then
  echo "Refusing to provision: incomplete stack outputs or non-isolated topic root"
  exit 1
fi

OUT_DIR="$OUTPUT_ROOT/$THING_NAME"
PRIVATE_KEY="$OUT_DIR/device-private-key.pem.key"
if [[ -e "$PRIVATE_KEY" ]]; then
  echo "Refusing to overwrite existing private key: $PRIVATE_KEY"
  exit 1
fi

mkdir -p "$OUT_DIR"
chmod 700 "$OUT_DIR"

ENDPOINT="$(aws iot describe-endpoint \
  --region "$REGION" \
  --endpoint-type iot:Data-ATS \
  --query endpointAddress \
  --output text)"

aws iot create-keys-and-certificate \
  --region "$REGION" \
  --set-as-active \
  --certificate-pem-outfile "$OUT_DIR/device-certificate.pem.crt" \
  --public-key-outfile "$OUT_DIR/device-public-key.pem.key" \
  --private-key-outfile "$PRIVATE_KEY" \
  --query '{certificateArn:certificateArn,certificateId:certificateId}' \
  > "$OUT_DIR/certificate.json"

chmod 600 "$PRIVATE_KEY" "$OUT_DIR/certificate.json"
chmod 644 "$OUT_DIR/device-certificate.pem.crt" "$OUT_DIR/device-public-key.pem.key"

curl --fail --silent --show-error \
  https://www.amazontrust.com/repository/AmazonRootCA1.pem \
  -o "$OUT_DIR/AmazonRootCA1.pem"

# certificate.json contains only ARN/ID because PEM and keys were written to
# separate files. Read the ID without printing either credential.
CERTIFICATE_ID="$(jq -r '.certificateId' "$OUT_DIR/certificate.json")"
CERTIFICATE_ARN="$(jq -r '.certificateArn' "$OUT_DIR/certificate.json")"

cat > "$OUT_DIR/device.json" <<EOF
{
  "region": "$REGION",
  "endpoint": "$ENDPOINT",
  "port": 8883,
  "thingName": "$THING_NAME",
  "mqttClientId": "$THING_NAME",
  "vehicleId": "$VEHICLE_ID",
  "topicPrefix": "$TOPIC_ROOT",
  "certificateId": "$CERTIFICATE_ID",
  "policyName": "$POLICY_NAME"
}
EOF
chmod 600 "$OUT_DIR/device.json"

cat <<EOF
Dedicated CACHE-001 certificate created but not attached.

Credential directory: $OUT_DIR
Certificate ARN: $CERTIFICATE_ARN

Next: update CloudFormation parameter TestCertificateArn with this ARN so the
stack, not this script, owns the policy and Thing attachments.
EOF
