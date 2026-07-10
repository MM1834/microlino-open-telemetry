# MQTT

MOT uses plain MQTT over TCP for the current firmware-to-broker connection. The browser dashboard may use MQTT over WebSocket, but that is a separate connection path and port.

## Topic pattern

```text
<mqttPrefix>/<vehicleId>/<category>/<field>
```

Typical default shape:

```text
mot/<vehicle>/<category>/<field>
```

## Telemetry topics

### Display

| Suffix | Type | Unit |
|---|---:|---|
| `display/soc` | number | % |
| `display/speed_kmh` | number | km/h |
| `display/odometer_km` | number | km |
| `display/estimated_range_km` | number | km |

### Charging

| Suffix | Type |
|---|---:|
| `charging/is_charging` | boolean |
| `charging/plugged` | boolean |
| `charging/power_display` | number |
| `charging/power_signed` | number |

### Location

| Suffix | Type | Unit |
|---|---:|---|
| `location/latitude` | number | degrees |
| `location/longitude` | number | degrees |
| `location/speed_kmph` | number | km/h |
| `location/satellites` | integer | — |
| `location/hdop` | number | — |
| `location/age_ms` | integer | ms |

### System

The current dashboard work expects these system suffixes:

| Suffix | Purpose |
|---|---|
| `system/device_id` | Device identity |
| `system/device_name` | Human-readable name |
| `system/mqtt_client_id` | MQTT client identifier |
| `system/firmware_version` | Firmware release |
| `system/ip_address` | Current interface IP |
| `system/network_mode` | `WiFi` or `LTE` |
| `system/mqtt_transport` | MQTT transport |
| `system/wifi_rssi` | WiFi signal |
| `system/uptime_sec` | Uptime |

## Retained values

State values are generally retained so a dashboard can reconstruct the latest known state immediately after subscribing.

## Presence

A true device-online state should be based on device telemetry, heartbeat and ideally an MQTT Last Will, not solely on the browser's MQTT connection.

A practical topic is:

```text
<mqttPrefix>/<vehicleId>/status/online
```

The dashboard can additionally use a broker-side `last_seen` timestamp generated whenever a device topic is received.

## Client IDs

Firmware device IDs should remain stable. Browser dashboard IDs should be persisted in browser storage instead of creating a new random ID on every page load.

## PubSubClient states

Use the state text produced by the current firmware as the primary interpretation. Historic documents used different numeric mappings during development, so old sprint notes must not be treated as authoritative for current return-code numbers.
