# Hardware comparison

| Feature | ESP32-WROOM + SN65HVD230 | WeAct ESP32 CAN485 | LilyGO T-A7670G |
|---|---:|---:|---:|
| ESP32 | Yes | Yes | Yes |
| CAN receive | Yes | Yes | Yes |
| Integrated CAN transceiver | No | Yes | No / external CAN wiring |
| WiFi MQTT | Yes | Yes | Yes |
| LTE MQTT | No | No | Experimental |
| GPS | Optional | Optional | External L76K |
| Local WebUI | Yes | Yes | Yes |
| OTA | Yes | Yes | Yes |
| Backup/Restore | Yes | Yes | Yes |
| Recommended first setup | Yes | Yes | No, unless LTE is required |

Start with the ESP32-WROOM or WeAct CAN485 if you want the most predictable path. Use LilyGO when LTE/GPS are required or when developing the mobile transport.
