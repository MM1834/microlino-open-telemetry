# MOT Dashboard

The MOT Dashboard is a lightweight web dashboard for Microlino Open Telemetry.

It is intentionally built without a framework so it can be hosted on simple webspace, a Raspberry Pi, NAS, nginx, Apache or any static hosting provider.

## Current Status

Prototype.

## Data Source

The dashboard connects directly from the browser to a MQTT broker using MQTT over WebSocket.

The MQTT broker must expose a WebSocket listener.

Example Mosquitto configuration:

```conf
listener 1883
protocol mqtt

listener 9001
protocol websockets
```

A production setup should use TLS via a reverse proxy such as Caddy or nginx.

## Files

```text
public/
  index.html
  css/style.css
  js/app.js
  manifest.json
```
