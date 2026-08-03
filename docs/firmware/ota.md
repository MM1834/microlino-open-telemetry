# OTA firmware update

For ESP32-WROOM under FW-SEC-001, OTA is available only when it is explicitly
enabled and the request passes the device's local administrator authentication and
same-origin check. Missing or invalid local credentials fail closed. LilyGO has not
yet received this hardening and must not be issued externally with an empty OTA
password.

## Recommended procedure

1. Sign in to the local WebUI as `admin` using the device-specific password.
2. Export the non-secret configuration backup.
3. Keep USB recovery access available.
4. Build the intended board environment.
5. Enable local OTA deliberately in Config.
6. Upload the firmware binary through the authenticated WebUI.
7. Allow the device to reboot.
8. Verify firmware version, network, MQTT/AWS, CAN and GPS.
9. Disable OTA again when the support operation is complete unless the device's
   support policy explicitly keeps it enabled.

## Recovery

Beta devices should retain a physical USB recovery option. OTA must not be the only way to recover a unit.

## Security

Do not expose the local OTA interface directly to the public internet. Local Basic
authentication is accepted only inside the WPA2-protected/local network boundary;
it is not a substitute for HTTPS on an Internet-facing service. Firmware signing,
Secure Boot and flash encryption remain later hardening work.
