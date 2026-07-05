# LilyGO MQTT Debug

Adds:

```text
GET /api/lilygo/mqtt/debug
```

The endpoint performs a direct TCP connect test to the configured MQTT host/port and returns WiFi status, IP, gateway, DNS, RSSI, MQTT host/port, TCP result, TCP duration and PubSubClient state.

If `tcpConnectOk=false`, the ESP32 does not reach the broker TCP port.
If `tcpConnectOk=true`, but MQTT does not connect, investigate MQTT protocol/auth/client-id.
