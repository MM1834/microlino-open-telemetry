# LilyGO T-A7670G

> **Status:** Source-declared board assembly; current hardware behaviour unverified
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
| CAN RX/TX | 32/13 | External transceiver plan |

Pin declarations do not certify wiring, voltage levels, termination, power supply
or connector pinout. Review the exact assembly before vehicle connection.

## Transport status

- WiFi network and local AP code are present.
- AWS IoT build code uses WiFi only.
- Modem registration/GPRS and LTE client code are present.
- Legacy MQTT contains an LTE fallback candidate.
- Current LTE MQTT/TLS stability is unverified and not beta-ready by documentation.

The dependable field path reported by the maintainer is currently a vehicle/phone
WiFi hotspot. This operational observation still requires a recorded current-head
test before being called validated.

## Related documents

- [LTE firmware status](../firmware/lte.md)
- [LilyGO CAN wiring plan](lilygo-can-sn65hvd230.md)
- [Hardware comparison](comparison.md)
