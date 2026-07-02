# MOT Dashboard

The MOT Dashboard is a static Progressive Web App. It can be hosted on any web server and connects to telemetry data via MQTT over WebSocket.

## Requirements

- MQTT broker with WebSocket support
- MQTT topics following `docs/MQTT.md`
- Browser-accessible `ws://` or `wss://` endpoint

## Quick Start

1. Copy the `dashboard/` directory to a web server.
2. Replace `dashboard/js/mqtt.min.js` with the MQTT.js browser bundle.
3. Open `index.html`.
4. Configure WebSocket URL, topic prefix, username and password.
5. Press **Connect**.

Example topic prefix:

```text
mot/pioneer
```

Example full topic:

```text
mot/pioneer/display/soc
```
