# Hardware Comparison

> **Status:** Source-based capability comparison; hardware validation pending
>
> **Audience:** Maintainer and hardware reviewer

| Area | ESP32-WROOM assembly | WeAct CAN485 option | LilyGO T-A7670G assembly |
|---|---|---|---|
| Firmware family | ESP32-WROOM | ESP32-WROOM-compatible intent; exact support needs review | LilyGO T-A7670G |
| CAN controller | ESP32 TWAI | ESP32 TWAI | ESP32 TWAI |
| CAN transceiver | External | Integrated on board | External SN65HVD230 plan |
| Source pins | RX27/TX26 | Hardware-specific mapping must be confirmed | RX32/TX13 |
| WiFi | Present | Expected, unverified | Present |
| LTE/GPRS | No | No | Modem/network code present; beta readiness unverified |
| GPS | Optional UART | Optional subject to pins | External L76K |
| AWS IoT | WiFi AWS environment | Not independently defined | AWS environment uses WiFi only |
| Local WebUI/OTA | Present | Expected, unverified | Present |

The ESP32-WROOM is the intended first beta platform, with or without GPS. This does
not certify a particular enclosure, transceiver module or vehicle connector.

## Vehicle-model limitation

Current implemented decoding targets Display CAN. Standard-CAN support is an empty
template. Supporting additional models requires a separately reviewed electrical
interface and verified signal definitions.

## Related documents

- [ESP32-WROOM](esp32-wroom.md)
- [LilyGO](lilygo-t-a7670.md)
- [CAN pipeline](../firmware/can.md)
