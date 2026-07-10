# Dashboard

![Dashboard and status](../assets/images/webui/dashboard-status.png)

## Purpose

The dashboard gives a quick overview of the current device state. It is the first page to check after flashing, restoring a backup or starting a test drive.

## What to check

| Area | Expected state |
|---|---|
| Firmware | Shows the current firmware version and board |
| Network | Shows WiFi or LTE mode and IP address |
| MQTT | Shows connected state and broker information |
| GPS | Shows fix state, satellites and location if available |
| CAN | Shows ready state and received frame counters |
| ABRP | Shows whether ABRP is enabled/configured |

## Typical use

Use the dashboard to answer:

- Is the device running?
- Which network path is active?
- Is MQTT connected?
- Does GPS have a valid fix?
- Is CAN receiving frames?
- Are telemetry values being updated?

## Troubleshooting

| Symptom | Next page |
|---|---|
| MQTT not connected | [MQTT/CAN](mqtt-can.md) |
| LTE not online | [LTE](lte.md) |
| No GPS fix | [Live status](live-status.md) |
| CAN frames not increasing | [MQTT/CAN](mqtt-can.md) |
