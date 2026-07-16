# AWS-2.2 — Device presence and heartbeat

This package extends the LilyGO AWS WiFi/TLS build with a clear cloud presence contract.

## MQTT contract

### Last Will

The MQTT CONNECT registers this retained Last Will:

```text
mot/<vehicleId>/status/online = false
QoS 1
retained
```

AWS publishes it if the connection disappears unexpectedly.

### Birth messages

Immediately after a successful MQTT connection the firmware publishes retained:

```text
status/online = true
system/device_id
system/device_name
system/mqtt_client_id
system/firmware_version
system/network_mode
system/mqtt_transport
system/ip_address
system/boot_reason
system/last_seen_utc
```

It also sends an immediate heartbeat.

### Heartbeat

Every 30 seconds:

```text
mot/<vehicleId>/system/heartbeat
```

Non-retained JSON payload:

```json
{
  "utc": 1784125790,
  "uptime_sec": 123,
  "free_heap": 175888,
  "network_mode": "WiFi",
  "transport": "WiFi",
  "ip_address": "192.168.100.124",
  "wifi_rssi": -19
}
```

Normal telemetry and `system/last_seen_utc` continue every five seconds.

## AWS policy requirement

Retained publishes require both:

```json
"iot:Publish",
"iot:RetainPublish"
```

For an existing LilyGO Thing:

```bash
./tools/update_aws_iot_presence_policy.sh \
  eu-north-1 \
  mot-lilygo-fe8ce0
```

The `private/aws/<thing>/device.json` file must contain `policyName`,
`accountId` and `vehicleId`, as created by the bootstrap tooling.

## Build and upload

```bash
cd firmware/lilygo-t-a7670
pio run -e T-A7670X-AWS
pio run -e T-A7670X-AWS -t upload
pio device monitor
```

Credentials in LittleFS do not need to be uploaded again unless they changed.

## AWS MQTT test-client subscriptions

```text
mot/pioneer/#
$aws/events/presence/+/mot-lilygo-fe8ce0
```

Expected:

- stable connection,
- `status/online = true`,
- heartbeat every 30 seconds,
- `last_seen_utc` every telemetry interval,
- `status/online = false` after an ungraceful power/network loss.

## Power-loss test

1. Subscribe to `mot/pioneer/status/online`.
2. Confirm retained `true`.
3. Remove LilyGO power without a graceful MQTT disconnect.
4. AWS should publish retained `false` after detecting the lost connection.
5. Restore power; the firmware should publish retained `true` after reconnect.
