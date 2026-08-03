# AWS-2.1 — LilyGO WiFi/TLS transport

## Credential layout

Per-device credentials are independent from firmware source and live at the
repository top level:

```text
private/aws/<thing-name>/
```

They are ignored by Git.

The upload helper stages them temporarily into the LilyGO LittleFS image,
uploads the filesystem, and removes the staged copies afterward.

## Prepare credentials

```bash
./tools/copy_aws_credentials_to_private.py mot-lilygo-fe8ce0
```

Or copy these files manually:

```text
AmazonRootCA1.pem
device-certificate.pem.crt
device-private-key.pem.key
device.json
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run -e T-A7670X-AWS
```

## Upload credentials

Connect the LilyGO over USB, then from repository root:

```bash
./tools/upload_lilygo_aws_credentials.py mot-lilygo-fe8ce0
```

## Upload firmware

```bash
cd firmware/lilygo-t-a7670
pio run -e T-A7670X-AWS -t upload
pio device monitor
```

## Expected serial sequence

```text
AWS IoT: enabled endpoint=... port=8883 clientId=mot-lilygo-fe8ce0 credentials=LittleFS
UTC: NTP synchronization requested via WiFi
AWS IoT: connecting ...
AWS IoT: connected
```

The AWS build intentionally uses WiFi only. It never selects the LilyGO LTE
client. The existing `lilygo-t-a7670` environment remains the plain MQTT debug
build.

## Important

LittleFS upload may replace an existing filesystem partition. This firmware
currently serves its WebUI from compiled C++ strings, but always take a config
backup before filesystem or firmware operations.
