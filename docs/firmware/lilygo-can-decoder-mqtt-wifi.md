# LilyGO CAN Decoder + MQTT over WiFi

Adds:

- TWAI frame conversion to shared `MotCanFrame`
- Shared decoder engine integration
- Shared telemetry model and telemetry JSON
- MQTT publish over WiFi using PubSubClient
- `/api/telemetry`
- `/api/lilygo/mqtt`

MQTT is WiFi-only in this step. LTE MQTT follows later because the current LTE layer is AT-only.
