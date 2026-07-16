# AWS-2.3 telemetry contract

- **Status:** Initial contract
- **Scope:** Firmware, AWS ingestion, application backend and dashboard
- **Compatibility namespace:** `mot/<vehicleId>/...`

## Principles

1. Topic names are stable interfaces.
2. Firmware publishes device/vehicle state; the dashboard does not infer broker state as vehicle state.
3. Retained messages represent the latest state.
4. Heartbeats are live events and are not retained.
5. The application backend preserves topic/key meaning even if storage changes.
6. Device credentials are never exposed to the browser.

## Presence

| Topic suffix | Payload | Retained | Purpose |
|---|---|---:|---|
| `status/online` | `true` / `false` | Yes | MQTT birth/Last Will |
| `system/last_seen_utc` | Unix UTC seconds | Yes | Last successful device update |
| `system/heartbeat` | JSON | No | Liveness and runtime diagnostics |

Heartbeat example:

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

## System state

| Topic suffix | Type | Retained |
|---|---|---:|
| `system/device_id` | string | Yes |
| `system/device_name` | string | Yes |
| `system/mqtt_client_id` | string | Yes |
| `system/firmware_version` | string | Yes |
| `system/network_mode` | string | Yes |
| `system/mqtt_transport` | string | Yes |
| `system/ip_address` | string | Yes |
| `system/boot_reason` | string | Yes |
| `system/wifi_rssi` | integer | Yes |
| `system/uptime_sec` | integer | Yes |

## Display telemetry

| Topic suffix | Type | Unit | Retained |
|---|---|---|---:|
| `display/soc` | number | % | Yes |
| `display/speed_kmh` | number | km/h | Yes |
| `display/odometer_km` | number | km | Yes |
| `display/estimated_range_km` | number | km | Yes |

## Charging

| Topic suffix | Type | Retained |
|---|---|---:|
| `charging/is_charging` | boolean/0/1 | Yes |
| `charging/plugged` | boolean/0/1 | Yes |
| `charging/power_display` | integer | Yes |
| `charging/power_signed` | integer | Yes |

## Location

| Topic suffix | Type | Unit | Retained |
|---|---|---|---:|
| `location/latitude` | number | degrees | Yes |
| `location/longitude` | number | degrees | Yes |
| `location/speed_kmph` | number | km/h | Yes |
| `location/satellites` | integer | — | Yes |
| `location/hdop` | number | — | Yes |
| `location/age_ms` | integer | ms | Yes |

## Backend event contract

The future WebApp backend should emit live updates as:

```json
{
  "topic": "mot/pioneer/system/last_seen_utc",
  "payload": 1784125790
}
```

A snapshot endpoint should return either:

```json
{
  "values": {
    "status/online": true,
    "system/last_seen_utc": 1784125790,
    "display/soc": 73
  }
}
```

or the `values` object directly.

## Compatibility

The dashboard provider normalizes both Legacy MQTT and the future AWS backend into the same `(topic, payload)` callback. Existing UI mapping therefore remains unchanged.
