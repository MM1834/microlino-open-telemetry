# ESP32-WROOM

The ESP32-WROOM setup is the reference WiFi hardware path. It is inexpensive, simple to debug and ideal for the first working installation.

![ESP32-WROOM](../assets/images/hardware/esp32-wroom.png)

![ESP32-WROOM board top](../assets/images/hardware/esp32-wroom-board-top.png)

## CAN wiring

| Signal | ESP32 GPIO | Notes |
|---|---:|---|
| CAN RX | GPIO32 | From SN65HVD230 RXD |
| CAN TX | GPIO13 | To SN65HVD230 TXD |
| GND | GND | Common ground |
| 3.3 V | 3V3 | Depending on CAN module |

## Advantages

- Simple and inexpensive.
- Stable WiFi MQTT path.
- Good development baseline.
- Easy to flash and debug.

## Limitations

- No integrated LTE.
- Requires separate CAN transceiver module.
- GPS requires extra hardware.
