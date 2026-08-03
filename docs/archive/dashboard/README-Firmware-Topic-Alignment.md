# MOT Firmware Topic Alignment

This patch aligns the WROOM firmware MQTT topic structure with the dashboard target format:

```text
mot/<vehicleId>/display/soc
mot/<vehicleId>/display/speed_kmh
mot/<vehicleId>/display/odometer_km
mot/<vehicleId>/display/estimated_range_km
mot/<vehicleId>/charging/is_charging
```

Configuration now separates:

- `vehicleName`: display name only, e.g. `Microlino Pioneer`
- `vehicleId`: stable MQTT topic id, e.g. `pioneer`
- `mqttPrefix`: namespace, usually `mot`

After flashing, open the firmware Web UI and set:

```text
MQTT Prefix: mot
Vehicle ID: pioneer
Vehicle Name: Microlino Pioneer
```

Existing older configs are migrated if possible, but checking the Web UI once is recommended.
