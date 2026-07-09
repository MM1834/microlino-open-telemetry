# WebUI troubleshooting

## Cannot open WebUI

Check:

- Are you connected to the device AP?
- Is the device connected to WiFi and do you know its IP?
- Did the device reboot?
- Is the USB serial log showing startup complete?

## MQTT not connected

Check:

- Host and port.
- Username and password.
- MQTT client ID.
- Broker logs.
- Whether WiFi or LTE is active.

## No CAN frames

Check:

- CAN RX/TX pins.
- CAN-H/CAN-L wiring.
- Common ground.
- Vehicle awake state.
- CAN bitrate.

## No GPS fix

Check:

- GPS antenna position.
- Sky visibility.
- GPS serial wiring.
- Whether `seen` is true.

## LTE connected but MQTT timeout

This is a known experimental area for the LilyGO LTE path. If WiFi/hotspot works but LTE MQTT reports `rc=-4`, treat it as an LTE transport issue rather than a vehicle telemetry issue.

## Backup restore looks wrong

After restore, reboot the device and check:

- Device name.
- Vehicle ID.
- MQTT broker.
- WiFi/LTE settings.
- OTA password.
