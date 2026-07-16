# AWS-3.5 — ESP32-WROOM second vehicle

## Provision a second vehicle identity

Use a separate Thing and vehicle ID, for example:

```bash
./tools/bootstrap_aws_iot_thing.sh \
  eu-north-1 \
  mot-esp32-f924f0 \
  beta-01
```

Ensure the policy contains `iot:RetainPublish`.

Copy credentials:

```bash
./tools/copy_aws_credentials_to_private.py mot-esp32-f924f0
```

## Build

```bash
cd firmware/esp32-wroom
pio run -e esp32dev-aws
```

## Upload credentials

From repository root:

```bash
./tools/upload_aws_credentials.py \
  mot-esp32-f924f0 \
  esp32-wroom
```

## Upload firmware

```bash
cd firmware/esp32-wroom
pio run -e esp32dev-aws -t upload
pio device monitor
```

Expected:

```text
AWS IoT: configured endpoint=... thing=mot-esp32-f924f0 vehicle=beta-01
AWS IoT: connected
```

The Dashboard selector should then list:

```text
pioneer · online
beta-01 · online
```
