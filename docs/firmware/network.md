# Network Management

> **Status:** Source-confirmed paths; runtime and failover unverified
>
> **Audience:** Firmware developer and beta-support author

## Common local network behaviour

All current targets can start a setup/fallback access point and can attempt a configured WiFi
station connection. After initial provisioning, the operational AP and local
administration use the configured local administrator password.

## ESP32-WROOM

The network manager selects WiFi station mode when connected and otherwise reports
the fallback AP address/mode. AWS IoT and legacy MQTT both depend on WiFi/network
online state.

## ESP32-C6

The C6 network loop is cooperative: station connection and 15-second reconnect
attempts do not wait inside the loop and therefore do not stop Dual-CAN, GPS,
WebUI or AWS processing. With no configuration it starts a protected setup AP.
If a configured station is unavailable for 20 seconds, the protected fallback AP
starts while station reconnect continues.

Before local administration is configured, the device-specific setup password is
printed on USB serial and protects both WPA2 AP access and the `setup` WebUI
account. Afterwards the local administrator password protects the fallback AP and
the `admin` account. This slice stores one station profile; WIFI-001 owns ordered
Home/mobile profiles.

## LilyGO

The network/modem source includes WiFi and LTE/GPRS state. WiFi is preferred and
LTE is inactive when no APN is configured. Transport behaviour is build-path
specific:

- AWS IoT: selects WiFi first and the A7670 X.509/TLS client as fallback;
- legacy MQTT: chooses WiFi first and LTE when WiFi is unavailable and GPRS is up.

LTE registration uses bounded attempts, exponential backoff and modem recovery
after repeated failures. Hardware soak and adverse-condition qualification remain
open.

## Security and support

- do not expose the AP/WebUI to the Internet;
- treat configuration export as potentially secret;
- avoid collecting WiFi SSIDs/passwords in public support logs;
- validate AP naming, reconnect and recovery on beta hardware before publishing a
  user procedure.

## Related documents

- [Firmware overview](overview.md)
- [LTE status](lte.md)
- [Local device API](../api/local-device-api.md)
