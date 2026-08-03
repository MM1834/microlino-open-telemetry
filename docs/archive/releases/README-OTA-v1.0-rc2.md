# MOT OTA v1.0 RC2

Adds Web OTA update support for the ESP32-WROOM firmware.

## New routes

- `GET /update` - OTA upload page
- `GET /ota` - alias for `/update`
- `POST /update` - firmware upload endpoint

## Security

If `config.otaPassword` is set in the firmware Web UI, OTA uses HTTP Basic Auth:

- username: `admin`
- password: the configured OTA password

For beta testers, set an OTA password before handing over the device.

## Install

From repository root:

```bash
unzip ~/Downloads/mot-ota-v1.0-rc2.zip
cp -R mot-ota-v1.0-rc2/. .
rm -rf mot-ota-v1.0-rc2

python3 tools/apply_ota_webui_patch.py

cd firmware/esp32-wroom
pio run
```

Then upload/flash once via USB.

After that, OTA is available at:

```text
http://<device-ip>/update
```

Upload the PlatformIO binary:

```text
firmware/esp32-wroom/.pio/build/esp32dev/firmware.bin
```

## Notes

- This patch keeps the existing Web UI and only registers additional OTA routes.
- The ESP32 must have enough free flash space for OTA. The default ESP32 DevKit partition scheme is usually sufficient for typical firmware sizes, but if OTA fails due to space, use an OTA-capable partition table.
