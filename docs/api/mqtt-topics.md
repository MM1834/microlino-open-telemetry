# MQTT Topic Contract

> **Status:** Topic suffixes confirmed in current source; payload/runtime unverified
>
> **Audience:** Firmware, backend and integration developer

## Namespace

Both firmware targets publish below:

```text
mot/<vehicleId>/<suffix>
```

Legacy MQTT allows a configured prefix but defaults to `mot`. AWS IoT credentials
provide `vehicleId` and `topicPrefix`, also defaulting to `mot`.

## Vehicle telemetry

| Suffix | Value | ESP32 | LilyGO | Notes |
|---|---|---:|---:|---|
| `display/soc` | number | Yes | Yes | percent-like value from current decoder |
| `display/speed_kmh` | number | Yes | Yes | km/h |
| `display/odometer_km` | number | Yes | Yes | km |
| `display/estimated_range_km` | integer | Yes | Yes | derived from SOC/current fixed full range |
| `charging/is_charging` | boolean-like | Yes | Yes | derived threshold; calibration noted in code |
| `charging/plugged` | boolean-like | Yes | Yes | Display-CAN flag |
| `charging/power_display` | integer | Yes | Yes | raw/display value |
| `charging/power_signed` | integer (0.1 kW) | Yes | Yes | vehicle convention: consumption positive, charge/regen negative; Display-only fallback remains decoder-derived |

Legacy MQTT encodes booleans as `1`/`0`; AWS IoT encodes them as JSON-style
`true`/`false` strings. Consumers must not assume identical boolean payload text
across transports.

## Standard-CAN battery telemetry

WROOM, LilyGO and both C6 AWS build profiles share these suffixes. Values are
published only after the relevant decoder validity gate passes.

| Suffix | Value/unit | Pioneer status |
|---|---|---|
| `bms/pack_voltage` | V | Confirmed |
| `bms/pack_current` | A | Confirmed; positive into battery, negative out of battery |
| `bms/pack_power_w` | W | battery convention: positive charge/regen, negative discharge |
| `bms/vehicle_power_w` | W | vehicle convention: positive consumption, negative charge/regen |
| `bms/is_regenerating` | boolean | confirmed from current direction while unplugged |
| `bms/is_discharging` | boolean | confirmed from current direction below -2 A |
| `bms/status_byte` | raw integer | `0x10` unplugged, `0x20` plugged confirmed |
| `bms/cell_min_mv` | mV | Provisional `0x4AD` pair |
| `bms/cell_max_mv` | mV | Provisional `0x4AD` pair |
| `bms/cell_delta_mv` | mV | Provisional derived delta |

The C6 AWS profiles are repository-build validated but not yet physically
provisioned or connected to AWS. See the
[Pioneer decoder evidence](../firmware/pioneer-standard-can.md).

## Location

Published only while the board reports a valid GPS fix:

| Suffix | ESP32 legacy/AWS | LilyGO legacy | LilyGO AWS |
|---|---:|---:|---:|
| `location/latitude` | Yes | Yes | Yes |
| `location/longitude` | Yes | Yes | Yes |
| `location/speed_kmph` | Yes | Yes | Yes |
| `location/satellites` | Yes | Yes | Yes |
| `location/hdop` | Yes | No | Yes when numeric |
| `location/age_ms` | Yes | No | Yes |

The spelling `speed_kmph` is part of the current contract even though
`speed_kmh` would be more conventional. Renaming requires a versioned migration.

AWS location topics are retained. When no current fix exists, firmware does not
overwrite retained coordinates; backend metadata/receive time must distinguish
current from last-known location.

## System topics published by board telemetry loops

| Suffix | ESP32 AWS | LilyGO AWS | Legacy |
|---|---:|---:|---:|
| `system/device_id` | Yes | Yes | LilyGO yes; ESP32 legacy does not publish it in current loop |
| `system/device_name` | Yes | Yes | LilyGO yes; ESP32 legacy does not publish it in current loop |
| `system/mqtt_client_id` | Birth message | Yes plus birth | AWS birth; not common legacy contract |
| `system/firmware_version` | Yes | Yes | LilyGO yes |
| `system/ip_address` | Yes | Yes | LilyGO yes |
| `system/network_mode` | Yes | Yes | LilyGO yes |
| `system/mqtt_transport` | `WiFi` | `WiFi` | LilyGO active WiFi/LTE transport |
| `system/wifi_rssi` | Yes | Yes | LilyGO yes |
| `system/uptime_sec` | Yes | Yes | LilyGO yes |
| `system/last_seen_utc` | Yes | Yes | AWS shared transport only |

The shared AWS transport may publish system identity fields both during its birth
sequence and later telemetry cycles.

## AWS presence and heartbeat

The shared `MotAwsIot` client adds:

| Suffix | Retained | Behaviour |
|---|---:|---|
| `status/online` | Yes | Last Will `false`; birth `true` |
| `system/boot_reason` | Yes | Reset reason on birth |
| `system/heartbeat` | No | Periodic JSON heartbeat/runtime summary |
| `system/last_seen_utc` | Yes | UTC timestamp when device time is valid |

No current topic named `status/lastSeen` or `status/transport` is published. Older
documentation recommending those names was a future proposal, not implementation.

## Retention

Telemetry helpers currently publish retained values for most telemetry/system
suffixes. Heartbeat is non-retained. Consumers should use received metadata and
presence/last-seen fields rather than equating a retained value with fresh data.

## Decoder limitation

Pioneer Standard-CAN publishes only the evidence-bounded fields above. V2 uses
the same candidate layout but remains unverified on a V2 vehicle. Display-CAN SOC
continues to be authoritative for Pioneer because `0x18D data[7]` is not its
displayed SOC.

## Related documents

- [AWS architecture](../architecture/aws-iot.md)
- [Firmware architecture](../firmware/architecture.md)
