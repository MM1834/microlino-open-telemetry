# MQTT Specification

This document defines the MQTT topic structure for Microlino Open Telemetry (MOT).

MQTT mirrors the MOT telemetry model.

## Base Topic

Recommended base topic:

```text
mot/<vehicle-id>/
```

Examples:

```text
mot/pioneer/display/soc
mot/blue/display/odo
mot/MOT-A34F12/system/uptime
```

The `<vehicle-id>` should be configurable.

For simple installations it may be:

```text
microlino
```

For multi-vehicle setups it should be unique.

## Retained Messages

Telemetry values should be published as retained messages unless they are high-frequency debug data.

This allows dashboards and integrations to show the latest known state immediately after connecting.

## Payload Format

Payloads are plain text values unless otherwise specified.

Examples:

```text
74.5
```

```text
1
```

```text
192.168.11.124
```

Boolean values are published as:

```text
1
0
```

## Display Topics

| Topic | Type | Unit | Retained | Description |
|---|---:|---:|---:|---|
| `mot/<vehicle-id>/display/soc` | float | % | yes | State of charge from Display CAN |
| `mot/<vehicle-id>/display/speed` | float | km/h | yes | Vehicle speed from Display CAN |
| `mot/<vehicle-id>/display/odo` | float | km | yes | Odometer from Display CAN |
| `mot/<vehicle-id>/display/range` | int | km | yes | Estimated range calculated from SOC |

## Charging Topics

| Topic | Type | Unit | Retained | Description |
|---|---:|---:|---:|---|
| `mot/<vehicle-id>/charging/is_charging` | bool | - | yes | Charging active |
| `mot/<vehicle-id>/charging/power_display` | int | raw | yes | Raw power display value from Display CAN |
| `mot/<vehicle-id>/charging/plugged_unconfirmed` | bool | - | yes | Candidate plug state, not yet fully confirmed |

## Vehicle Topics

| Topic | Type | Unit | Retained | Description |
|---|---:|---:|---:|---|
| `mot/<vehicle-id>/vehicle/name` | string | - | yes | Configured vehicle name |
| `mot/<vehicle-id>/vehicle/model` | string | - | yes | Optional model description |

## System Topics

| Topic | Type | Unit | Retained | Description |
|---|---:|---:|---:|---|
| `mot/<vehicle-id>/system/device_id` | string | - | yes | MOT device ID |
| `mot/<vehicle-id>/system/firmware` | string | - | yes | Firmware version |
| `mot/<vehicle-id>/system/ip` | string | - | yes | Current IP address |
| `mot/<vehicle-id>/system/mac` | string | - | yes | WiFi MAC address |
| `mot/<vehicle-id>/system/rssi` | int | dBm | yes | WiFi signal strength |
| `mot/<vehicle-id>/system/uptime` | int | s | yes | Device uptime |
| `mot/<vehicle-id>/system/network_mode` | string | - | yes | Current network state |

## BMS Topics - Planned

| Topic | Type | Unit | Retained | Description |
|---|---:|---:|---:|---|
| `mot/<vehicle-id>/bms/voltage` | float | V | yes | Battery voltage |
| `mot/<vehicle-id>/bms/current` | float | A | yes | Battery current |
| `mot/<vehicle-id>/bms/power` | float | kW | yes | Battery power |
| `mot/<vehicle-id>/bms/temperature_min` | float | °C | yes | Minimum battery temperature |
| `mot/<vehicle-id>/bms/temperature_max` | float | °C | yes | Maximum battery temperature |
| `mot/<vehicle-id>/bms/cell_voltage_min` | float | V | yes | Minimum cell voltage |
| `mot/<vehicle-id>/bms/cell_voltage_max` | float | V | yes | Maximum cell voltage |

## GPS Topics - Planned

| Topic | Type | Unit | Retained | Description |
|---|---:|---:|---:|---|
| `mot/<vehicle-id>/gps/latitude` | float | degrees | yes | GPS latitude |
| `mot/<vehicle-id>/gps/longitude` | float | degrees | yes | GPS longitude |
| `mot/<vehicle-id>/gps/altitude` | float | m | yes | GPS altitude |
| `mot/<vehicle-id>/gps/speed` | float | km/h | yes | GPS speed |
| `mot/<vehicle-id>/gps/valid` | bool | - | yes | GPS fix valid |

## Debug Topics - Optional

Debug topics should not be required by normal users.

Examples:

```text
mot/<vehicle-id>/debug/can/last_id
mot/<vehicle-id>/debug/can/frame_count
mot/<vehicle-id>/debug/heap/free
```

## Compatibility Rule

Once a stable topic is released, it should not be renamed without a migration period.

New values should be added under the existing hierarchy whenever possible.
