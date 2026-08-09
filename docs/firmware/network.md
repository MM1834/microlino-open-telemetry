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

The C6 network loop is cooperative and uses two ordered station profiles. It tries
the preferred Home-WiFi first and then the second/mobile hotspot after a bounded
15-second timeout. If neither configured profile connects, the protected fallback
AP starts and the preferred sequence retries after 30 seconds. This does not block
Dual-CAN, GPS, WebUI or AWS processing.

While the mobile profile is active, an asynchronous scan checks for Home-WiFi every
60 seconds and switches back when the preferred SSID is visible. The fallback AP
stays available during association and is stopped only after 10 seconds of stable
station connectivity. Repository tests and builds pass; physical transition and
concurrent-service validation remain open in WIFI-001.

Before local administration is configured, the device-specific setup password is
printed on USB serial and protects both WPA2 AP access and the `setup` WebUI
account. Afterwards the local administrator password protects the fallback AP and
the `admin` account. The WebUI, backup/import and serial console store both
profiles without rendering or logging their passwords. Runtime diagnostics report
the active profile, state and transition reason.

XIAO hardware testing found that the ESP32-C6 driver can briefly retain
`WL_CONNECTED` after hotspot loss while its local address is already `0.0.0.0`.
The online predicate therefore requires both connected status and a non-zero local
address before a profile is considered usable.

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
