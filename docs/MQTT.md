# MQTT Specification

MOT publishes telemetry using this structure:

```text
<prefix>/<vehicleId>/<category>/<value>
```

Default:

```text
mot/pioneer/...
```

## Configuration

| Setting | Default | Description |
|---|---|---|
| MQTT Prefix | `mot` | Root topic for all MOT devices |
| Vehicle ID | `pioneer` | Stable technical ID used in topics |
| Vehicle Name | `Microlino Pioneer` | Human-readable name for UI only |

## Display topics

| Topic | Type | Unit | Description |
|---|---:|---|---|
| `mot/<vehicleId>/display/soc` | float | % | State of charge |
| `mot/<vehicleId>/display/speed_kmh` | float | km/h | Vehicle speed |
| `mot/<vehicleId>/display/odometer_km` | float | km | Odometer |
| `mot/<vehicleId>/display/estimated_range_km` | integer | km | Estimated range |

## Charging topics

| Topic | Type | Unit | Description |
|---|---:|---|---|
| `mot/<vehicleId>/charging/is_charging` | boolean | - | Charging active candidate |
| `mot/<vehicleId>/charging/plugged` | boolean | - | Plugged-in state, if available |
| `mot/<vehicleId>/charging/power_display` | integer | raw | Display CAN power value |
| `mot/<vehicleId>/charging/power_signed` | integer | raw | Signed power candidate |

## System topics

| Topic | Type | Unit | Description |
|---|---:|---|---|
| `mot/<vehicleId>/system/device_id` | string | - | ESP32 device ID |
| `mot/<vehicleId>/system/firmware_version` | string | - | Firmware version |
| `mot/<vehicleId>/system/ip_address` | string | - | Current IP address |
| `mot/<vehicleId>/system/network_mode` | string | - | WiFi STA or fallback AP |
| `mot/<vehicleId>/system/wifi_rssi` | integer | dBm | WiFi signal strength |
| `mot/<vehicleId>/system/uptime_sec` | integer | s | Runtime since boot |

## Retained messages

Recommended:

- Retain slowly changing state such as SOC, odometer and system information.
- Do not retain high-frequency debug data.

## Security

For dashboards served over HTTPS, MQTT over WebSocket must use WSS.

Never expose unauthenticated MQTT directly to the internet.
