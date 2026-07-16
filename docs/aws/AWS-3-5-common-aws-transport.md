# AWS-3.5 — Common AWS transport

AWS-3.5 introduces a shared PlatformIO library:

```text
firmware/shared-libs/MotAwsIot
```

It owns:

- LittleFS credential loading,
- `WiFiClientSecure`,
- PubSubClient setup,
- AWS MQTT CONNECT,
- retained Last Will and Birth message,
- heartbeat,
- UTC validation,
- reconnect handling,
- common publish helpers,
- common topic construction.

Board-specific firmware owns only:

- network availability,
- device/runtime metadata,
- telemetry selection,
- board-specific services.

## First consumer

The ESP32-WROOM AWS build now uses the shared library:

```text
env: esp32dev-aws
```

The existing plain MQTT environment remains:

```text
env: esp32dev
```

for local debugging only.

## LilyGO migration

The running LilyGO AWS build is deliberately not overwritten in this package.
Its current AWS implementation remains stable while the ESP32-WROOM validates
the shared library with a second physical vehicle.

After that test, LilyGO can be changed to the same `MotAwsIot` library without
changing its modem/LTE code. The AWS build remains WiFi-only until LTE MQTT is
stabilized.

## Credentials

Credentials remain device-specific and outside firmware source:

```text
private/aws/<thing-name>/
```

They are uploaded temporarily to LittleFS using:

```bash
./tools/upload_aws_credentials.py \
  <thing-name> \
  esp32-wroom
```
