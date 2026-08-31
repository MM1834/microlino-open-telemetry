# Network Management

> **Status:** REV5 repository and N16 Mobile-to-Home field acceptance complete
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
60 seconds and attempts the preferred profile whenever its SSID is visible. RSSI
is retained as diagnostic evidence but no longer blocks Home priority: a mobile
hotspot may keep a strong WiFi link and DHCP address while its cellular uplink is
unavailable. If the visible Home network cannot actually be joined, the bounded
15-second timeout returns to Mobile. A connected Home link at or below `-88 dBm`
is reported as weak after 20 seconds, but RSSI alone does not disconnect it. The
fallback AP stops after any station connection remains stable for ten seconds.

`transportReady` follows real station/IP connectivity rather than the diagnostic
weak-RSSI flag. This preserves local administration and Internet transports on a
weak but working Home link and prevents Home–Mobile–AP oscillation when Mobile is
unavailable.
AWS TLS handshakes are bounded to seven seconds and socket waits to five seconds;
failed connects use
exponential backoff from 10 seconds up to five minutes. ABRP retains its separate
task and now also rejects the stale `WL_CONNECTED`/zero-IP condition.

Runtime diagnostics expose link weakness duration and transition count plus reset
reason, current/minimum heap and largest free block. REV4 N16 hardware testing
confirmed that Home remained connected and AWS published at approximately
`-85 dBm`, with Mobile unavailable and no forced profile transition.

REV5 restores the product rule that Home has priority whenever its configured
SSID is visible. It removes only the REV4 `-80 dBm` Mobile-to-Home admission gate;
the weak-link diagnostics, bounded association timeout and loss-driven fallback
remain unchanged. Physical N16 acceptance subsequently passed: the manager moved
from Mobile to Home, associated at `-71 dBm`, restored AWS and completed ABRP with
HTTP 200 while Dual-CAN and GPS continued without errors.

The REV3 diagnostic follow-up additionally reports the connected access point's
BSSID and channel, cumulative station disconnects, the latest ESP32 disconnect
reason and whether it followed a manager-requested disconnect. It also reports the
age of the latest profile attempt and a cumulative AWS connect-failure count. This
allows a remote pilot to distinguish mesh-node loss or roaming from an intentional
Home/Mobile transition without changing the accepted thresholds.

Mesh systems do not guarantee that the ESP32 station will roam seamlessly between
BSSIDs. If the currently associated node disappears before another node is usable,
the normal bounded Home→Mobile→fallback-AP recovery policy applies. BSSID and
channel diagnostics are therefore evidence for later infrastructure tuning, not a
separate firmware-controlled roaming mechanism.

Before local administration is configured, the device-specific setup password is
printed on USB serial and protects both WPA2 AP access and the `setup` WebUI
account. Afterwards the local administrator password protects the fallback AP and
the `admin` account. The WebUI, backup/import and serial console store both
profiles without rendering or logging their passwords. Runtime diagnostics report
the active profile, state, transition reason, link health and runtime memory/reset
evidence.

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
