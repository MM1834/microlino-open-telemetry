# MOT v1.1.0 – LilyGO MQTT Debug

## Einspielen

```bash
unzip ~/Downloads/mot-v1.1.0-lilygo-mqtt-debug.zip
cp -R mot-v1.1.0-lilygo-mqtt-debug/. .
rm -rf mot-v1.1.0-lilygo-mqtt-debug

python3 tools/apply_v1_1_0_lilygo_mqtt_debug.py
```

## Build / Upload

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/mqtt
/api/lilygo/mqtt/debug
```
