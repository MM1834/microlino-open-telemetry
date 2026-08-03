> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# MQTT Diagnostics

MOT v1.0.2 adds firmware-side MQTT diagnostics.

## API

```text
GET /api/mqtt-test
```

Example response:

```json
{
  "host": "mmds.muehlberg.ch",
  "port": 33025,
  "resolvedIp": "203.0.113.10",
  "wifiConnected": true,
  "dnsOk": true,
  "tcpOk": true,
  "mqttOk": true,
  "mqttState": 0,
  "message": "MQTT login successful",
  "durationMs": 521
}
```

## PubSubClient return codes

| Code | Meaning |
|---:|---|
| 0 | MQTT connected |
| -1 | connection timeout |
| -2 | TCP connect failed |
| -3 | connection lost |
| -4 | disconnected |
| 1 | bad protocol |
| 2 | bad client id |
| 3 | server unavailable |
| 4 | bad credentials |
| 5 | unauthorized |

The firmware uses plain MQTT over TCP. Do not use the dashboard WebSocket/WSS port for the ESP32 firmware.
