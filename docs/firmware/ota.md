# OTA firmware update

OTA is available through the local WebUI and should be password protected.

## Recommended procedure

1. Export a configuration backup.
2. Keep USB recovery access available.
3. Build the intended board environment.
4. Upload the firmware binary through the WebUI.
5. Allow the device to reboot.
6. Verify firmware version, network, MQTT, CAN and GPS.
7. Export a fresh backup if the configuration schema changed.

## Recovery

Beta devices should retain a physical USB recovery option. OTA must not be the only way to recover a unit.

## Security

OTA authentication, AP protection and WebUI authentication will be reviewed together in the security sprint. Do not expose the local OTA interface directly to the public internet.
