# MOT Dashboard v1.0 – Default Location

This patch adds a configurable default location for the dashboard map/location card.

Default coordinates:

- Latitude: `47.46198`
- Longitude: `8.11068`

The dashboard uses this location until real GPS/location MQTT topics are available.

Supported live MQTT topics:

```text
mot/<vehicle>/location/latitude
mot/<vehicle>/location/longitude
mot/<vehicle>/location/lat
mot/<vehicle>/location/lon
mot/<vehicle>/gps/latitude
mot/<vehicle>/gps/longitude
mot/<vehicle>/gps/lat
mot/<vehicle>/gps/lon
```

Configuration in `dashboard/config.js`:

```js
vehicle: {
  defaultLocation: {
    enabled: true,
    latitude: 47.46198,
    longitude: 8.11068,
    label: "Default location"
  }
}
```

Set `enabled: false` to hide the default location until GPS data arrives.
