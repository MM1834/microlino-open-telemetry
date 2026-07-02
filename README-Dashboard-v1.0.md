# MOT Dashboard v1.0

Cockpit-style dashboard for Microlino Open Telemetry.

## Install

Copy the contents of `dashboard/` to your web server dashboard directory.

Required: replace `dashboard/libs/mqtt.min.js` with the working MQTT.js browser build.

## Configure

Edit `dashboard/config.js`:

```js
host: "mmds.muehlberg.ch",
port: 20226,
useTls: true,
topicPrefix: "mot",
vehicleId: "pioneer"
```

Expected MQTT base topic:

```text
mot/pioneer/#
```

## Supported live topics

```text
mot/pioneer/display/soc
mot/pioneer/display/speed_kmh
mot/pioneer/display/odometer_km
mot/pioneer/display/estimated_range_km
mot/pioneer/charging/is_charging
mot/pioneer/charging/power_display
mot/pioneer/charging/power_signed
mot/pioneer/system/device_id
mot/pioneer/system/firmware_version
mot/pioneer/system/network_mode
mot/pioneer/system/ip_address
mot/pioneer/system/wifi_rssi
mot/pioneer/system/uptime_sec
mot/pioneer/location/lat
mot/pioneer/location/lng
mot/pioneer/location/address
```

If location topics are missing, the dashboard shows the configured fallback map position.
