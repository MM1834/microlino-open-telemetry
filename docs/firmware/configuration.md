# Firmware configuration

Configuration is stored in ESP32 Preferences/NVS and can be edited through the local WebUI.

## Main settings

| Setting | Purpose |
|---|---|
| `deviceName` | Stable device identity and MQTT client-name source |
| `vehicleId` | Vehicle component in MQTT topic paths |
| `mqttPrefix` | Root MQTT topic prefix |
| `wifiSsid` / `wifiPass` | WiFi station credentials |
| `lteApn` / user / password | Mobile packet-data configuration |
| `mqttHost` / `mqttPort` | MQTT broker endpoint |
| `mqttUser` / `mqttPass` | MQTT credentials |
| `otaEnabled` / `otaPassword` | OTA configuration |
| `abrpEnabled` | Enables optional ABRP integration |
| ABRP API key/token | ABRP credentials |

## Stable identity

The device name should remain stable after deployment. It is used in status information and may be used to derive the MQTT client ID.

Use a unique value for every physical unit.

## Optional services

MQTT should only be started when a broker host is configured. ABRP should only be started when it is enabled and its required credentials are present.

## Configuration workflow

1. Configure the device through the WebUI.
2. Verify WiFi or LTE status.
3. Verify MQTT over WiFi first.
4. Export a JSON backup.
5. Store the backup securely.
