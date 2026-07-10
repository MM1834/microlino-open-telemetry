# Backup, Restore and Factory Reset

The WebUI exports configuration as JSON and can restore that JSON to the device.

## Included fields

The current documented schema includes:

- device and vehicle identity,
- MQTT prefix,
- WiFi credentials,
- LTE APN and credentials,
- MQTT endpoint and credentials,
- OTA enable/password,
- ABRP enable/API key/user token.

## Not included

- runtime counters,
- CAN frame counters,
- live GPS fix,
- current telemetry,
- connection state.

## Restore

After importing a backup:

1. Reboot the device.
2. Verify identity and network settings.
3. Verify MQTT.
4. Export a new backup after validation.

## Factory Reset

Factory Reset clears configuration stored in Preferences/NVS. It does not erase the installed firmware or WebUI assets.

After reset, the device returns to initial setup behavior and exposes its local AP.

## Security

Backups contain secrets. Never commit real backup JSON files, publish them in bug reports or store them in a public repository.
