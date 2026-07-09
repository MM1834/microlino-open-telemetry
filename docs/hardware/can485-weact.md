# WeAct Studio ESP32 CAN485

The WeAct Studio ESP32 CAN485 board is a compact alternative to the classic ESP32-WROOM plus external SN65HVD230 module.

![WeAct CAN485 top](../assets/images/hardware/weact-can485-top.png)

![WeAct CAN485 bottom](../assets/images/hardware/weact-can485-bottom.png)

## Compatibility

| Item | Value |
|---|---|
| MCU | ESP32-WROOM compatible |
| CAN transceiver | SN65HVD230 |
| CAN RX | GPIO32 |
| CAN TX | GPIO13 |
| CAN bitrate | 500 kbit/s |
| RS485 | Not used by MOT |

Because the CAN transceiver uses the same GPIOs as the reference ESP32 setup, the WeAct board can use the ESP32-WROOM firmware path without a pin change.

## Validation checklist

Expected WebUI CAN status:

```json
{
  "ready": true,
  "rxPin": 32,
  "txPin": 13,
  "bitrate": "500k"
}
```
