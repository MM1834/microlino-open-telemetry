# LilyGO T-A7670G

The LilyGO T-A7670G is the mobile MOT platform with ESP32, SIMCom A7670G LTE modem and external L76K GPS.

![LilyGO board top](../assets/images/hardware/lilygo-t-a7670-board-top.png)

![LilyGO board bottom](../assets/images/hardware/lilygo-t-a7670-board-bottom.png)

![LilyGO with L76K GPS](../assets/images/hardware/lilygo-t-a7670-board-top-incl-l76k-gps-module.png)

## Current status

| Function | Status |
|---|---:|
| Local WebUI/AP | Working |
| WiFi MQTT | Working |
| GPS | Working |
| CAN diagnostics | Working |
| Backup/Restore | Working |
| OTA | Working |
| LTE registration/GPRS | Working |
| LTE MQTT | Experimental |
| ABRP over LTE HTTPS | Deferred |

## Pin usage

| Function | GPIO | Notes |
|---|---:|---|
| Modem RX | 27 | ESP32 RX from modem |
| Modem TX | 26 | ESP32 TX to modem |
| Modem PWR | 4 | Power key |
| Board POWER_ON | 12 | Board power enable |
| Modem RST | 5 | Reset |
| Modem DTR | 25 | DTR |
| Modem RI | 33 | Ring indicator |
| GPS RX | 22 | L76K GPS |
| GPS TX | 21 | L76K GPS |
| CAN RX | 32 | SN65HVD230 |
| CAN TX | 13 | SN65HVD230 |

## LTE note

The modem can register, attach to GPRS and open TCP connections. MQTT over LTE is still marked experimental because the broker sees a connection while the firmware times out waiting for MQTT CONNACK. WiFi via phone hotspot is currently the recommended field-test path.
