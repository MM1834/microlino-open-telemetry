# LilyGO AWS firmware update validation

> **Status:** USB update and WiFi/AWS validation passed; LTE/AWS and local OTA
> hardening remain open
>
> **Date:** 2026-08-03

## Scope

The existing LilyGO T-A7670 device was updated to the reviewed
`T-A7670X-AWS` application without erasing NVS or replacing its LittleFS AWS
identity. The exact USB target, ESP32 family and firmware-derived device suffix
were confirmed before the write.

## Evidence

| Check | Result |
|---|---|
| 64 automated tests before hardware write | Passed |
| `T-A7670X-AWS` build | Passed |
| RAM | 53,976 / 327,680 bytes (16.5%) |
| Application flash | 1,132,757 / 1,310,720 bytes (86.4%) |
| USB application upload and hash verification | Passed |
| Existing NVS configuration retained | Passed |
| Existing LittleFS AWS identity retained | Passed |
| Thing certificate active and exclusively bound | Passed |
| Thing `boardType` corrected to `lilygo-t-a7670` | Passed |
| WiFi/AWS reconnect, online birth and heartbeat | Passed |
| Firmware and MQTT client identity in vehicle state | Passed |
| GPS fix and fresh location metadata | Passed; coordinates omitted here |

The local ignored credential directories and private key files were restricted to
directory mode `700` and key mode `600`. No credential content was printed or
committed.

## Transport finding

With the configured WiFi unavailable, the modem obtained an LTE/GPRS connection
and mobile address. The current shared AWS client nevertheless remained offline
because the LilyGO AWS path is WiFi-only. AWS IoT over the LilyGO LTE/TLS client is
therefore a separate implementation and validation block, not a configuration fix.

## Portal freshness finding

The device was not connected to vehicle CAN during validation. Retained SOC and
display values correctly remained visible, but the portal initially labelled the
WebSocket channel as `Live` and derived OBD2 freshness from the current device
heartbeat. This made old CAN values appear live.

The corrected beta portal:

- labels WebSocket connectivity as `Live-Kanal`;
- preserves backend `receivedAt` for REST snapshots and WebSocket updates;
- derives Microlino/OBD2 freshness only from decoded display-CAN topics;
- displays the SOC value timestamp and marks an old value as `veraltet`;
- leaves current GPS and device connectivity independent from CAN freshness.

The hosted beta retest passed with a connected live channel, current GPS, stale
OBD2 status and the retained SOC visibly marked as stale.

## Remaining gates

- Harden LilyGO local WebUI, fallback AP and OTA to the WROOM security boundary.
- Validate the hardened LilyGO paths on hardware before external handoff.
- Implement and test AWS IoT X.509 over LTE/TLS separately.
- Validate fresh CAN values and automatic freshness recovery on the vehicle.
