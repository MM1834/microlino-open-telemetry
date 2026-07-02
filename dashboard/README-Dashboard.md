# MOT Dashboard v0.3

Static dashboard for Microlino Open Telemetry.

## Setup

1. Replace `js/mqtt.min.js` with the browser bundle from MQTT.js:
   `https://unpkg.com/mqtt/dist/mqtt.min.js`
2. Edit `config.js`:

```js
window.MOT_CONFIG = {
  host: "your-domain.example",
  port: 22026,
  useTls: false,
  username: "",
  password: "",
  topicPrefix: "mot",
  vehicleId: "pioneer"
};
```

3. Upload the folder contents to your webhost.

## MQTT topics

The dashboard subscribes to:

```text
mot/<vehicleId>/#
```

Expected example topics:

```text
mot/pioneer/display/soc
mot/pioneer/display/speed_kmh
mot/pioneer/display/odometer_km
mot/pioneer/display/estimated_range_km
mot/pioneer/charging/is_charging
mot/pioneer/charging/power_display
mot/pioneer/system/device_id
mot/pioneer/system/firmware_version
mot/pioneer/system/ip_address
mot/pioneer/system/wifi_rssi
mot/pioneer/system/uptime_sec
```
