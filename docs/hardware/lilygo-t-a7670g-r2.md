# LilyGO T-A7670G R2

MOT v1.1.0 starts support for the LilyGO T-A7670G R2 ESP32 4G Development Board with GNSS.

## CAN pin plan

The A7670G modem uses ESP32 GPIO26/GPIO27 for its modem UART.

Therefore MOT must not use the WROOM CAN pins 26/27 on the LilyGO target.

For the Microlino Display CAN via SN65HVD230:

```text
CAN RX: GPIO32
CAN TX: GPIO33
```

## CAN bus count

The ESP32 has one internal TWAI/CAN controller. The LilyGO variant therefore supports one native CAN bus.

A second CAN bus requires an external CAN controller, for example MCP2515 via SPI.

## Sprint 1 scope

Do not connect the Microlino/CAN bus in Sprint 1.

Sprint 1 tests only:

- modem boot
- SIM status
- network registration
- GPRS attach status
- GNSS raw AT response
- local WiFi setup AP
- diagnostic web endpoints
