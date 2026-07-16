# AWS-3.5.1 — Dashboard vehicle switching

Fixes vehicle-specific state handling:

- topic parsing uses the selected vehicle, not the configured default,
- old SOC, ODO, range, charging, IP, network and location values are cleared,
- `system/firmware_version` and `system/wifi_rssi` are recognized,
- history is assigned to the selected vehicle,
- stale HTTP responses from the previous vehicle are ignored.

Cloud/API connectivity stays active while switching vehicles.
