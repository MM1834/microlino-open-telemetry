> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO TinyGSM A76XX Transport

Switches the MOT LilyGO LTE MQTT transport from the experimental custom AT socket stack to the LewisXhe TinyGSM fork used by obd2mqtt.

Reference result from obd2mqtt:

```text
TinyGSM Version: 1.0.0
TinyGsmClientA76xxSSL
Modem: A7670G-LLSE
MQTT ... connected
```

## Key changes

- Uses `https://github.com/lewisxhe/TinyGSM`
- Defines `TINY_GSM_MODEM_A76XXSSL`
- Replaces `src/lte/lilygo_lte_client.*`
- Keeps MOT Web UI, OTA, CAN, GPS, config and MQTT high-level logic unchanged.
- Adds `/api/lilygo/lte/mqtt-trace`.

## Notes

This intentionally avoids the earlier custom commands that were incompatible on the tested firmware:

- `AT+CIPSTATUS` returned `ERROR`
- `AT+CIPRXGET` returned `+IP ERROR: operation not supported`
