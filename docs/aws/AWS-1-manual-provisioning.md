# AWS-1: Manual AWS IoT bootstrap

This package prepares one manually provisioned beta device.

## Recommended first device

Use one ESP32-WROOM test unit over WiFi.

Example identity:

```text
Thing name : mot-esp32-f924f0
Client ID  : mot-esp32-f924f0
Vehicle ID : pioneer
Region     : eu-central-1
```

The Thing name is immutable in AWS IoT, so choose it carefully.

## Prerequisites

```bash
aws --version
jq --version
aws sts get-caller-identity
```

Configure AWS CLI using an IAM identity intended for administration. Device firmware never receives these AWS administrator credentials.

## Protect credentials in Git

Append the supplied ignore rules:

```bash
cat .gitignore.aws-iot >> .gitignore
git add .gitignore
```

Verify:

```bash
git check-ignore -v secrets/aws-iot/test/private.key
```

## Create the first Thing

```bash
./tools/bootstrap_aws_iot_thing.sh   eu-central-1   mot-esp32-f924f0   pioneer
```

The command creates:

- Thing
- active device certificate
- private key
- per-device IoT policy
- exclusive certificate attachment
- Amazon Root CA
- local `device.json`

## Console validation

In AWS IoT Core:

```text
Test
-> MQTT test client
-> Subscribe to a topic
```

Subscribe to:

```text
mot/pioneer/#
```

## Desktop TLS smoke test

Before modifying firmware, validate the generated credentials with a desktop MQTT client such as Mosquitto:

```bash
THING=mot-esp32-f924f0
DIR=secrets/aws-iot/$THING
ENDPOINT=$(jq -r .endpoint "$DIR/device.json")

mosquitto_pub   --host "$ENDPOINT"   --port 8883   --cafile "$DIR/AmazonRootCA1.pem"   --cert "$DIR/device-certificate.pem.crt"   --key "$DIR/device-private-key.pem.key"   --id "$THING"   --topic mot/pioneer/status/bootstrap_test   --message '{"ok":true}'   --qos 1
```

The message should appear in the AWS MQTT test client.

## Policy scope

The generated policy permits:

- connect only with the exact Thing name as MQTT client ID,
- publish only below `mot/<vehicleId>/`,
- subscribe/receive only below the device's command and configuration branches.

The first firmware implementation only needs publishing, Last Will and heartbeat. Command subscriptions can remain unused until a later sprint.

## Cleanup

Do not delete the local private key before securing a backup.

To deactivate a compromised certificate:

```bash
aws iot update-certificate   --region <region>   --certificate-id <certificate-id>   --new-status INACTIVE
```
