# MQTT Topics

All topics use:

```text
mot/<vehicle>/<category>/<field>
```

## Display

| Topic suffix | Type | Unit | Description |
|---|---:|---|---|
| display/soc | number | % | State of charge |
| display/speed_kmh | number | km/h | Vehicle speed |
| display/odometer_km | number | km | Odometer |
| display/estimated_range_km | number | km | Estimated range |

## Charging

| Topic suffix | Type | Unit | Description |
|---|---:|---|---|
| charging/is_charging | boolean | - | Charging state |
| charging/plugged | boolean | - | Plug status |
| charging/power_display | number | - | Displayed charge power |
| charging/power_signed | number | - | Signed power value |

## System

| Topic suffix | Type | Unit | Description |
|---|---:|---|---|
| system/device_id | string | - | Device ID |
| system/ip | string | - | IP address |
| system/rssi | number | dBm | WiFi signal |
| system/uptime | number | s | Uptime |
