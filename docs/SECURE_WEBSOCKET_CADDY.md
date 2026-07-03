# Secure MQTT WebSocket with Caddy

When the dashboard is served over HTTPS, browsers require secure WebSockets (`wss://`).

A simple setup is to terminate TLS with Caddy and forward internally to the MQTT WebSocket listener.

## Example architecture

```text
Browser dashboard
  │
  │ wss://mmds.example.com:20226
  ▼
Caddy reverse proxy
  │
  │ ws://ioBroker:2026
  ▼
ioBroker MQTT WebSocket
```

## Example Caddyfile

```caddy
mmds.example.com:20226 {
    reverse_proxy 192.168.1.10:2026
}
```

## Router forwarding

Forward:

```text
External 20226 → Caddy host 20226
```

For automatic Let's Encrypt certificate issuing, Caddy usually also needs access to port 80 or DNS challenge support.

## Dashboard configuration

```js
window.MOT_CONFIG = {
  mqtt: {
    host: "mmds.example.com",
    port: 20226,
    useTls: true,
    path: "/",
    username: "",
    password: "",
    topicPrefix: "mot",
    vehicleId: "pioneer"
  }
};
```

## Notes

Keep the internal ESP32 MQTT connection unchanged if possible.

The ESP32 can publish to the internal broker using plain MQTT, while the public dashboard uses WSS through Caddy.
