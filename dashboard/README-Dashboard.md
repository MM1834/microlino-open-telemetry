# MOT Dashboard v0.1

Static browser dashboard for Microlino Open Telemetry.

## Setup

Edit `dashboard/config.js` before uploading to your web host:

```js
window.MOT_CONFIG = {
  host: "your-mqtt-host.example.com",
  port: 22026,
  useTls: false,
  username: "",
  password: "",
  topicPrefix: "mot",
  vehicleId: "pioneer"
};
```

For the current test setup use `useTls: false` and your public WebSocket MQTT port.
Later, with TLS/Let's Encrypt, switch to `useTls: true` and usually `port: 443`.

## Upload

Upload the contents of the `dashboard/` folder to your web host.
Open `index.html` in a browser.

## MQTT topics

The dashboard subscribes to:

```text
<topicPrefix>/<vehicleId>/#
```

It expects values such as:

```text
mot/pioneer/display/soc
mot/pioneer/display/speed
mot/pioneer/display/odo
mot/pioneer/display/range
mot/pioneer/charging/is_charging
mot/pioneer/charging/power_display
mot/pioneer/system/ip
mot/pioneer/system/rssi
mot/pioneer/system/firmware
mot/pioneer/system/device_id
```

It also has fallback handling for older flat topics such as `microlino/display/soc` if configured accordingly.
