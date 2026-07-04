# ABRP Integration

MOT v1.0.3 adds optional ABRP telemetry.

The firmware follows the working Node-RED payload:

```json
{
  "soc": 88,
  "utc": 1750000000,
  "speed": 53,
  "power": 0.0,
  "is_charging": false
}
```

Endpoint:

```text
https://api.iternio.com/1/tlm/send
```

## Optional service

ABRP is disabled unless both values are configured:

- ABRP API Key
- ABRP User Token

Secrets are not logged and not published over MQTT.

## Firmware Web API

```text
GET  /api/abrp/status
POST /api/abrp/test
```
