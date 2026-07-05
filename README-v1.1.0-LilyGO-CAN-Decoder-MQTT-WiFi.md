# MOT v1.1.0 – LilyGO CAN Decoder + MQTT WiFi

## Einspielen

```bash
unzip ~/Downloads/mot-v1.1.0-lilygo-can-decoder-mqtt-wifi.zip
cp -R mot-v1.1.0-lilygo-can-decoder-mqtt-wifi/. .
rm -rf mot-v1.1.0-lilygo-can-decoder-mqtt-wifi

python3 tools/apply_v1_1_0_lilygo_can_decoder_mqtt_wifi.py
```

## Build / Upload

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/can
/api/lilygo/can/frames
/api/telemetry
/api/lilygo/mqtt
```

MQTT sendet in diesem Schritt nur über WiFi.
