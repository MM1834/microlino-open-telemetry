> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# System Health & MQTT Diagnostics

MOT v1.0.2 adds two firmware-side diagnostic endpoints:

```text
GET /api/mqtt-test
GET /api/system-health
```

## `/api/mqtt-test`

Checks the configured MQTT connection in layers:

- WiFi connected
- DNS resolution
- TCP port reachable
- MQTT login successful

## `/api/system-health`

Returns device, firmware and connectivity information:

- device id
- firmware version
- build date
- IP address
- RSSI
- uptime
- WiFi/DNS/TCP/MQTT state
- CAN validity state

## Web UI

The local ESP32 web UI now shows:

- **System Health prüfen** on the status page
- **Test Connection** in the MQTT section of the config page

## Notes

The ESP32 firmware uses plain MQTT over TCP. Do not use the dashboard WebSocket/WSS port for firmware MQTT.
