# Network Management

> **Status:** Source-confirmed paths; runtime and failover unverified
>
> **Audience:** Firmware developer and beta-support author

## Common local network behaviour

Both targets start a setup/fallback access point without a password and can attempt
a configured WiFi station connection. The AP provides local setup/recovery but is
not an authenticated security boundary.

## ESP32-WROOM

The network manager selects WiFi station mode when connected and otherwise reports
the fallback AP address/mode. AWS IoT and legacy MQTT both depend on WiFi/network
online state.

## LilyGO

The network/modem source includes WiFi and LTE/GPRS state. Transport behaviour is
build-path specific:

- AWS IoT: WiFi connectivity is passed to the shared AWS client;
- legacy MQTT: chooses WiFi first and LTE when WiFi is unavailable and GPRS is up.

Historical retry intervals and successful modem-layer experiments are not a public
guarantee for the current commit.

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
