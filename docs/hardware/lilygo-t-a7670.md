# LilyGO T-A7670G

> **Status:** Source-declared assembly with current functional field evidence
>
> **Audience:** Hardware reviewer and firmware developer

![LilyGO board top](../assets/images/hardware/lilygo-t-a7670-board-top.png)

![LilyGO board bottom](../assets/images/hardware/lilygo-t-a7670-board-bottom.png)

![LilyGO with L76K GPS](../assets/images/hardware/lilygo-t-a7670-board-top-incl-l76k-gps-module.png)

The board header identifies the T-A7670G R2/T-A7670X-GPS V1.1 assembly with an
ESP32-WROVER, SIMCom A7670G-LLSE modem and external L76K GPS.

## Declared pin usage

| Function | GPIO | Notes |
|---|---:|---|
| Modem RX/TX | 27/26 | ESP32 UART relative to modem |
| Modem power/PWRKEY | 4 | Board-specific control |
| Board power enable | 12 | `BOARD_POWER_ON_PIN` |
| Modem reset | 5 | Reset control |
| Modem DTR/RI | 25/33 | GPIO33 therefore avoided for CAN TX |
| GPS RX/TX | 22/21 | 9600 baud |
| GPS PPS/wakeup | 23/19 | Source-declared |
| CAN1 RX/TX | 36/13 | Standard-CAN; external transceiver; receive-only contract |
| CAN2 SPI | SCK 14, MOSI 32, MISO 39, CS 18 | Display-CAN; Adafruit MCP2515 FeatherWing; no SD card |
| CAN2 INT | 34 | Input-only MCP2515 interrupt/diagnostic line |
| CAN2 RST | 3.3 V | Required standalone pull-high via labeled FeatherWing pad |

Pin declarations do not certify wiring, voltage levels, termination, power supply
or connector pinout. Review the exact assembly before vehicle connection.

## Transport status

- WiFi is preferred; the operational local AP is WPA2-protected with the local
  administrator password.
- The AWS IoT build falls back to the modem TLS client when WiFi is unavailable.
- Modem registration/GPRS and LTE client code are present and functionally tested.
- Legacy MQTT contains an LTE fallback candidate.
- AWS IoT X.509 over LTE/TLS and live CAN/GPS delivery to the hosted portal passed
  on 2026-08-03. Long-duration, weak-signal and adverse-power qualification remain
  open.

ABRP remains WiFi-only and is not part of the validated LTE path.

## Dual-CAN pilot hardware

[LG-CAN2-001](../project/sprints/LG-CAN2-001.md) adds an Adafruit MCP2515 CAN Bus
FeatherWing as CAN2 on the otherwise unused SD SPI pins. The FeatherWing must be
powered from 3.3 V with common ground. Its `TERM` link must be open when tapping
the already terminated vehicle bus; the first pilot board has already been cut.
Tie `SLNT` permanently to 3.3 V while wiring so the TJA1051/3 transmitter stays
disabled independently of firmware. The MCP2515 is also configured in software
listen-only mode.

The logical assignment matches the C6 adapters: CAN1/native TWAI receives
Standard-CAN and CAN2/MCP2515 receives Display-CAN. Firmware migrates the former
single-CAN Display profile once to this new default mapping; verify both selected
profiles in the local configuration after migration.

Do not connect MCP2515 MISO to GPIO2. A physical test showed that the external
MISO level can alter this ESP32 strapping pin and prevent normal flash boot.
GPIO39 is the accepted input-only MISO connection.

GPIO15 is likewise excluded: physical testing showed that the FeatherWing MOSI
connection changed the ESP32 boot strap. GPIO32 is the accepted MOSI connection;
the former CAN1 RX wire moves from GPIO32 to input-only GPIO36.

Tie the FeatherWing's labeled `RST` pad to 3.3 V. Unlike a stacked Feather host,
the LilyGO does not otherwise hold the MCP2515 active-low reset line high.

The pilot uses two 1,664 KiB OTA slots and 640 KiB LittleFS. Moving an existing
adapter to this layout requires a USB full-image migration after configuration
and per-device credential backup; the former partition layout must not be updated
to this image through OTA.

## Related documents

- [LilyGO dual-CAN wiring diagram (PDF)](../assets/pdfs/hardware/lilygo-t-a7670g-dual-can-verkabelung.pdf)
- [LTE firmware status](../firmware/lte.md)
- [LilyGO CAN wiring plan](lilygo-can-sn65hvd230.md)
- [LilyGO mobile dual-CAN pilot](../project/sprints/LG-CAN2-001.md)
- [Hardware comparison](comparison.md)
