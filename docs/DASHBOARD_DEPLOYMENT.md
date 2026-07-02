# Dashboard deployment

## HTTPS and WSS

If the dashboard is served via HTTPS, the browser will block plain `ws://` connections.
Use WSS:

```text
https://www.muehlberg.ch/MOT/dashboard/
  -> wss://mmds.muehlberg.ch:20226
  -> ws://ioBroker:2026
```

## Caddy example

`/etc/caddy/Caddyfile`:

```caddy
mmds.muehlberg.ch:20226 {
    reverse_proxy 192.168.x.y:2026
}
```

Router forwarding:

```text
external 20226 -> Caddy Pi 20226
external 80    -> Caddy Pi 80    # for Let's Encrypt HTTP challenge
```

Dashboard config:

```js
host: "mmds.muehlberg.ch",
port: 20226,
useTls: true,
path: "/",
topicPrefix: "mot",
vehicleId: "pioneer"
```

## ioBroker MQTT adapter

Keep the internal MQTT setup unchanged if it already works.
For WebSockets, the ioBroker MQTT adapter uses the MQTT port + 1.
Example:

```text
MQTT port: 2025
WebSocket port: 2026
```

The ESP32 can continue to publish to the internal MQTT listener.
The public dashboard should use WSS through Caddy.
