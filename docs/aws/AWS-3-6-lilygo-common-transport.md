# AWS-3.6 — LilyGO on shared MotAwsIot

The LilyGO AWS WiFi build now uses the same shared `MotAwsIot` library as the
ESP32-WROOM.

Shared responsibility:

- credential loading from LittleFS,
- TLS/X.509,
- AWS MQTT connection,
- Last Will and Birth messages,
- heartbeat,
- UTC gating,
- reconnect,
- topic construction,
- publish helpers.

LilyGO responsibility:

- WiFi availability,
- network/IP/RSSI metadata,
- GPS and vehicle telemetry selection,
- LTE diagnostics.

The AWS build remains deliberately WiFi-only. The plain debug environment
retains WiFi/LTE broker behavior.

The obsolete duplicate files under `src/aws/` are removed by:

```bash
./tools/apply_aws_3_6_cleanup.py
```
