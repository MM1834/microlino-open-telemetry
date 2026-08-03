# AWS-3.6 LilyGO migration

## Apply

```bash
unzip ~/Downloads/aws-3-6-lilygo-common-aws.zip
./tools/apply_aws_3_6_cleanup.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run -e T-A7670X-AWS
```

Existing LittleFS credentials remain valid and do not need to be uploaded
again unless the filesystem was erased.

## Test

```bash
pio run -e T-A7670X-AWS -t upload
pio device monitor
```

Expected:

```text
AWS IoT: shared transport enabled ...
AWS IoT: connected ...
```

Verify in AWS/Dashboard:

- `status/online`
- `system/heartbeat`
- `system/last_seen_utc`
- GPS topics
- vehicle telemetry
- Last Will after power removal
