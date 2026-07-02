# MOT Telemetry Model

> One telemetry model. Multiple vehicles. Open platform.

The telemetry model is the single source of truth inside Microlino Open Telemetry.

CAN decoders, GPS modules and future inputs write into the telemetry model. Outputs such as MQTT, JSON API, dashboards and ABRP only read from it.

## Principles

1. Decoders do not publish MQTT.
2. Decoders do not build JSON.
3. Decoders do not know about dashboards or ABRP.
4. Outputs never decode CAN.
5. Every value should exist only once in the telemetry model.

## Top-level structure

```text
TelemetryState
├── vehicle
├── display
├── charging
├── bms
└── system
```

## Display telemetry

| Field | Type | Unit | Description |
|---|---:|---:|---|
| `display.valid` | bool | - | Display CAN data has been received |
| `display.soc` | float | % | State of charge |
| `display.speed` | float | km/h | Vehicle speed |
| `display.odo` | float | km | Odometer |
| `display.range` | int | km | Estimated range |

## Charging telemetry

| Field | Type | Unit | Description |
|---|---:|---:|---|
| `charging.valid` | bool | - | Charging data has been received |
| `charging.isCharging` | bool | - | Charging active |
| `charging.plugged` | bool | - | Cable plugged, confirmed source |
| `charging.pluggedUnconfirmed` | bool | - | Cable plugged candidate |
| `charging.powerDisplay` | int | raw | Display power value |
| `charging.powerSigned` | int | raw | Signed power candidate |

## BMS telemetry

BMS telemetry is prepared for future dual-CAN support.

| Field | Type | Unit | Description |
|---|---:|---:|---|
| `bms.valid` | bool | - | BMS data has been received |
| `bms.packVoltage` | float | V | Battery pack voltage |
| `bms.packCurrent` | float | A | Battery pack current |
| `bms.packPower` | float | kW | Battery power |
| `bms.minCellVoltage` | float | V | Minimum cell voltage |
| `bms.maxCellVoltage` | float | V | Maximum cell voltage |
| `bms.batteryTemperature` | float | °C | Battery temperature |

## System telemetry

| Field | Type | Unit | Description |
|---|---:|---:|---|
| `system.firmware` | string | - | Firmware version |
| `system.deviceId` | string | - | MOT device ID |
| `system.networkMode` | string | - | WiFi STA, fallback AP, LTE |
| `system.ip` | string | - | Current IP address |
| `system.mqttPrefix` | string | - | MQTT topic prefix |
| `system.rssi` | int | dBm | WiFi signal strength |
| `system.uptime` | uint32 | s | Uptime |

## MQTT mapping

The MQTT tree should mirror the telemetry model:

```text
mot/<vehicle>/display/soc
mot/<vehicle>/charging/is_charging
mot/<vehicle>/system/ip
```

## JSON mapping

The JSON API should mirror the same structure:

```json
{
  "display": {
    "soc": 74.5
  },
  "charging": {
    "is_charging": false
  }
}
```
