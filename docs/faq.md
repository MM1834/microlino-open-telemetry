# FAQ

## MQTT shows offline
Check WSS/WS settings, broker credentials and topic prefix.

## Browser blocks WebSocket
If the dashboard is served through HTTPS, the MQTT WebSocket must use WSS.

## OTA does not work
Check the OTA password, device IP and that the device is on the same network.

## Why only one CAN bus on ESP32-WROOM?
The ESP32-WROOM has one internal TWAI controller. A second CAN bus requires an external CAN controller such as MCP2515.
