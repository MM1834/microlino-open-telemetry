# MOT Dashboard v0.4 Cockpit

Copy the `dashboard/` folder to your web host. Replace `dashboard/js/mqtt.min.js` with the real MQTT.js browser build:

https://unpkg.com/mqtt/dist/mqtt.min.js

Edit `dashboard/config.js`:

```js
host: "mmds.muehlberg.ch",
port: 20226,
useTls: true,
username: "...",
password: "...",
topicPrefix: "mot",
vehicleId: "pioneer"
```

Expected topics:

- `mot/pioneer/display/soc`
- `mot/pioneer/display/speed_kmh`
- `mot/pioneer/display/odometer_km`
- `mot/pioneer/charging/is_charging`
- `mot/pioneer/charging/power_display`
- `mot/pioneer/system/firmware_version`
- `mot/pioneer/system/device_id`
- `mot/pioneer/system/network_mode`
- `mot/pioneer/system/rssi`
- `mot/pioneer/system/uptime`
