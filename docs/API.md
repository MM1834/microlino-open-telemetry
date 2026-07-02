# JSON API Specification

The MOT JSON API exposes the same data model as MQTT.

The API is intended for:

- embedded status pages
- dashboards
- debugging
- local integrations
- future cloud gateways

## Current Endpoint

```text
GET /api/status
```

## Response Structure

Example:

```json
{
  "system": {
    "project": "Microlino Open Telemetry",
    "firmware": "1.0.0-WROOM",
    "device_id": "MOT-A34F12",
    "uptime": 12345,
    "network_mode": "WiFi STA",
    "ip": "192.168.11.124",
    "rssi": -55
  },
  "vehicle": {
    "name": "Pioneer"
  },
  "display": {
    "valid": true,
    "soc": 74.5,
    "speed": 18.0,
    "odo": 12483.6,
    "range": 104
  },
  "charging": {
    "is_charging": false,
    "power_display": 0,
    "plugged_unconfirmed": false
  }
}
```

## Mapping to MQTT

The API hierarchy mirrors MQTT.

Example:

```json
"display": {
  "soc": 74.5
}
```

maps to:

```text
mot/<vehicle-id>/display/soc
```

## Field Categories

### system

Device and firmware information.

### vehicle

User-configured vehicle metadata.

### display

Values decoded from Microlino Display CAN.

### charging

Charging-related values.

### bms

Planned BMS values.

### gps

Planned GPS values.

## Confirmed vs Experimental Values

If a value is not fully confirmed, it should be named accordingly.

Example:

```json
"plugged_unconfirmed": true
```

This makes the status of reverse-engineered values clear to users and developers.

## Future Endpoints

Planned endpoints:

```text
GET /api/config
POST /api/config
GET /api/system
GET /api/can/status
GET /api/mqtt/status
POST /api/reboot
POST /api/factory-reset
```

## API Compatibility

Stable fields should not be removed or renamed without a migration period.

Experimental fields may change until marked stable.
