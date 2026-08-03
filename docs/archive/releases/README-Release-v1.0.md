# Microlino Open Telemetry v1.0 Release Package

This package contains the final v1.0 dashboard and the firmware topic-alignment patch.

## Included

- `dashboard/` — MOT Dashboard v1.0 cockpit UI
- `firmware/` — firmware files for the final MQTT topic structure
- `CHANGELOG.md` — v1.0 changelog entry
- `GITHUB_RELEASE_NOTES.md` — text for the GitHub release
- `docs/QUICK_RELEASE_GUIDE.md` — short finalization guide
- `docs/DASHBOARD_DEPLOYMENT.md` — dashboard + WSS deployment notes

## MQTT topic structure

The v1.0 target structure is:

```text
mot/<vehicleId>/display/soc
mot/<vehicleId>/display/speed_kmh
mot/<vehicleId>/display/odometer_km
mot/<vehicleId>/charging/is_charging
mot/<vehicleId>/system/firmware_version
```

Default values:

```text
MQTT Prefix: mot
Vehicle ID: pioneer
Vehicle Name: Microlino Pioneer
```

