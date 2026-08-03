# MOT LilyGO LTE MQTT Trace

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-mqtt-trace.zip
cp -R mot-lilygo-lte-mqtt-trace/. .
rm -rf mot-lilygo-lte-mqtt-trace

python3 tools/apply_lilygo_lte_mqtt_trace.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/lte/mqtt-trace
POST /api/lilygo/lte/mqtt-trace/clear
```
