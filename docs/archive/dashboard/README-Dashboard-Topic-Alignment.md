# MOT Dashboard Topic Alignment

This patch updates the dashboard to the target topic format:

```text
mot/<vehicleId>/...
```

It also accepts both the new firmware names and old aliases where useful:

- `display/speed_kmh` preferred, `display/speed` fallback
- `display/odometer_km` preferred, `display/odo` fallback
- `display/estimated_range_km` preferred, `display/range` fallback

Important: this package does **not** include `dashboard/js/mqtt.min.js`, so your already downloaded MQTT.js browser bundle will not be overwritten.
