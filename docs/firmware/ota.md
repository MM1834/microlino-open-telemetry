# OTA firmware update

For ESP32-WROOM and ESP32-C6 under FW-SEC-001, OTA is available only when it is explicitly
enabled and the request passes the device's local administrator authentication and
same-origin check. Missing or invalid local credentials fail closed. LilyGO now
implements and physically passes the same boundary; its hardened migration forces
OTA off until a provisioner explicitly enables it again.

## Recommended procedure

1. Sign in to the local WebUI as `admin` using the device-specific password.
2. Export the non-secret configuration backup.
3. Keep USB recovery access available.
4. Build the intended board environment.
5. Enable local OTA deliberately in Config.

REV13 validates the standard Espressif image header before opening the OTA
partition. A different ESP chip family or declared physical flash size is
rejected with HTTP 400, leaving the running firmware unchanged. This prevents
the 16 MB N16 and 4 MB XIAO C6 images from being interchanged. Boards with the
same chip family and flash geometry are not uniquely identified by this guard.
6. Upload the firmware binary through the authenticated WebUI.
7. Allow the device to reboot.
8. Verify firmware version, network, MQTT/AWS, CAN and GPS.
9. Disable OTA again when the support operation is complete unless the device's
   support policy explicitly keeps it enabled.

## Recovery

Beta devices should retain a physical USB recovery option. OTA must not be the only way to recover a unit.
On C6, WROOM and LilyGO firmware, `admin recover` on the 115200-baud physical
USB serial console replaces only a lost local administrator password with a new
random credential and prints it once. It does not clear network or decoder state.
An aborted upload invokes the update abort path. A rejected or invalid image does
not schedule a reboot, leaving the running application active. Successful uploads
reboot only after the HTTP result is returned. This is local recovery behaviour,
not a claim of signed-image rollback or fleet rollout.

## Security

Do not expose the local OTA interface directly to the public internet. Local Basic
authentication is accepted only inside the WPA2-protected/local network boundary;
it is not a substitute for HTTPS on an Internet-facing service. Firmware signing,
Secure Boot and flash encryption remain later hardening work.
