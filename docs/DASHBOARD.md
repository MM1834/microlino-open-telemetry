# Dashboard

The MOT dashboard is a static web dashboard using MQTT over WebSocket/WSS.

It can be hosted on any web server.

## Configuration

Copy:

```bash
cp dashboard/config.example.js dashboard/config.js
```

Then edit:

```js
window.MOT_CONFIG = {
  mqtt: {
    host: "mqtt.example.com",
    port: 443,
    useTls: true,
    path: "/",
    username: "",
    password: "",
    topicPrefix: "mot",
    vehicleId: "pioneer"
  }
};
```

## HTTPS and WSS

If the dashboard is served using HTTPS, browsers block plain `ws://` WebSocket connections.

Use:

```text
wss://mqtt.example.com
```

not:

```text
ws://mqtt.example.com
```

## MQTT.js

The dashboard requires the browser build of MQTT.js.

Download it and place it here:

```text
dashboard/libs/mqtt.min.js
```

Example source:

```text
https://unpkg.com/mqtt/dist/mqtt.min.js
```

## Deployment

Upload the content of `dashboard/` to your web host.

Do not commit personal `config.js` files containing private broker names or credentials.
