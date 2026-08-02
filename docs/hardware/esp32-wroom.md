# ESP32-WROOM

> **Status:** Intended beta reference; physical wiring and current firmware unverified
>
> **Audience:** Hardware reviewer, beta provisioner and firmware developer

![ESP32-WROOM](../assets/images/hardware/esp32-wroom.png)

![ESP32-WROOM board top](../assets/images/hardware/esp32-wroom-board-top.png)

## Source-declared interfaces

| Function | GPIO/configuration | Notes |
|---|---:|---|
| CAN RX | GPIO27 | From transceiver RXD |
| CAN TX | GPIO26 | To transceiver TXD |
| Optional GPS RX | GPIO16 | NMEA UART input |
| Optional GPS TX | GPIO17 | UART output/configuration path |
| GPS baud | 9600 | PlatformIO build flag |

An external CAN transceiver and correct vehicle-side wiring are required. Module
voltage compatibility, termination and grounding must be reviewed for the exact
beta assembly; this page is not an electrical installation approval.

## Intended beta variants

- ESP32-WROOM without GPS;
- ESP32-WROOM with optional GPS on the declared UART pins.

These are hardware options of one firmware line, not separate maintained firmware
generations. GPS availability is determined from valid NMEA input at runtime.

## Network and recovery

Source includes WiFi station operation, an open fallback AP, local WebUI,
configuration backup/restore, factory reset and browser OTA. Runtime behaviour must
be revalidated before devices are handed to beta testers.

## Related documents

- [Firmware overview](../firmware/overview.md)
- [CAN pipeline](../firmware/can.md)
- [Beta work order](../governance/WORK_ORDER.md)
