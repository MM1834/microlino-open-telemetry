> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# Firmware Notes for v1.0.1

MOT v1.0.1 is a dashboard-only release.

No firmware update is required if the device already runs MOT v1.0.0 and publishes the existing MQTT topic structure.

Required topics for full history functionality:

```text
mot/<vehicle>/display/soc
mot/<vehicle>/display/speed_kmh
mot/<vehicle>/display/estimated_range_km
mot/<vehicle>/charging/is_charging
mot/<vehicle>/charging/power_display
mot/<vehicle>/system/rssi
```
