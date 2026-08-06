# Firmware configuration

Configuration is stored in ESP32 Preferences/NVS and can be edited through the local WebUI.

Decoder assignment is a logical runtime setting, not a decoder restriction tied
to one controller. Multi-CAN targets store both `can1` and `can2`; single-CAN
targets retain Display-CAN as the default for their available input.

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
| `can1Profile` / `can2Profile` | Independent decoder assignment for each physical CAN input |

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

During C6 qualification, WiFi credentials can additionally be stored over the
115200-baud serial console with `wifi set <ssid>|<password>` and inspected without
secret echo using `wifi status`. `wifi clear` removes them. A restart applies a
new setting. This is a temporary provisioning surface, not a replacement for the
authenticated local administration acceptance gate.

The C6 AWS builds load the same four per-device credential files from
`/aws` in LittleFS as WROOM and LilyGO. `aws status` reports credential and
connection state without printing key material.
