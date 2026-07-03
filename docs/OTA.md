# OTA Updates

MOT supports browser-based OTA firmware updates on the ESP32-WROOM firmware.

## URL

Open:

```text
http://<esp-ip>/update
```

## Authentication

OTA upload is protected by the OTA password configured in the ESP32 Web UI.

Default username:

```text
admin
```

The password should be set during configuration.

## Build firmware

```bash
cd firmware/esp32-wroom
pio run
```

Firmware binary:

```text
.pio/build/esp32dev/firmware.bin
```

## Upload flow

1. Open `/update`
2. Login
3. Select `firmware.bin`
4. Upload
5. ESP32 flashes the firmware
6. ESP32 reboots automatically
7. Check version/status after reboot

## Beta testing recommendation

Before sending hardware to a beta tester:

- Flash once via USB.
- Configure WiFi/MQTT/OTA password.
- Test OTA upload locally.
- Record the OTA password securely.
- Verify that MQTT data appears after OTA reboot.
