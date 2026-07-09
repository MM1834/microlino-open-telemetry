# OBD and CAN connection

MOT reads the Microlino CAN bus. The current firmware operates in receive/diagnostic mode and does not send application CAN frames.

## CAN settings

| Setting | Value |
|---|---|
| Bus speed | 500 kbit/s |
| ESP32 peripheral | TWAI |
| Mode | Receive diagnostics |
| Application TX | Disabled |

## Wiring

| Vehicle side | Transceiver side |
|---|---|
| CAN High | CANH |
| CAN Low | CANL |
| Ground | GND |

Always verify CAN-H/CAN-L and ground before connecting hardware to the vehicle.
