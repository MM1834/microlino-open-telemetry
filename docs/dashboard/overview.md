# Dashboard Overview

![Desktop Home](../assets/images/dashboard/desktop-home.png)

The dashboard provides current and live telemetry through the configured provider.
For the AWS backend it renders authorized SOC, Speed, signed-power, charging and
plugged history for 24 hours, 7 days and 30 days. Speed and power gaps are closed
at zero in the chart rather than linearly interpolated across missing reception.
Legacy MQTT retains browser-local history as a fallback.

The repository portal also renders the Standard-CAN BMS topics
`bms/pack_voltage`, `bms/pack_current`, `bms/pack_power_w` and the provisional
`bms/cell_min_mv`, `bms/cell_max_mv` and `bms/cell_delta_mv` values. The generic
AWS state and WebSocket path requires no schema deployment for these live fields.
This repository support does not mean the updated static portal package has been
uploaded to the hosted site. The portal labels the two `0x4AD` values as
cell-voltage candidates because their status as true pack-wide extrema is not yet
independently confirmed.

Following the controlled Pioneer road test, the portal also understands
`bms/vehicle_power_w`, `bms/is_regenerating` and `bms/is_discharging`. The battery
card uses consumption-positive vehicle power while raw pack power retains the
battery convention (positive into the pack). This prevents charging and
regeneration from being displayed with the traction sign.

The live battery card presents charging power as a positive magnitude and changes
its label to `Ladeleistung` while charging. History presents the magnitude of the
signed average net power and labels the newest point as consumption, charging or
regeneration. Consumption and regeneration within one aggregation interval can
therefore partly cancel. The underlying signed vehicle-power topic and stored
History value remain negative for energy entering the battery, so this display
formatting does not alter aggregation semantics. The Speed chart marks its newest
measurement as not current after the expected sampling interval; this can mean
either standstill suppression or an offline device and is not by itself a
connectivity diagnosis.

## Main views
- Home
- Battery
- Vehicle
- Charging
- Temperatures
- Cells
- Location
