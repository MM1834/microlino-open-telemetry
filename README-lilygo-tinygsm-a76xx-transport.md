# MOT LilyGO TinyGSM A76XX Transport

## Apply from repo root

```bash
cd /Users/martin/Documents/MICROLINO/microlino-open-telemetry

unzip ~/Downloads/mot-lilygo-tinygsm-a76xx-transport.zip
cp -R mot-lilygo-tinygsm-a76xx-transport/. .
rm -rf mot-lilygo-tinygsm-a76xx-transport

python3 tools/apply_lilygo_tinygsm_a76xx_transport.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run -t clean
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/mqtt
/api/lilygo/lte/mqtt-trace
```

For first LTE validation, keep WiFi SSID empty/wrong and use the MQTT DNS hostname again.
