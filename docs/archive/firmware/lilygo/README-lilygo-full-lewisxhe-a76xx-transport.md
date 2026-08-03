# MOT LilyGO Full LewisXhe A76XX LTE Transport

This replaces MOT's experimental A7670 AT socket stack with one shared LewisXhe TinyGSM A76XXSSL transport, matching the working obd2mqtt transport family.

## Apply from repository root

```bash
cd /Users/martin/Documents/MICROLINO/microlino-open-telemetry

unzip ~/Downloads/mot-lilygo-full-lewisxhe-a76xx-transport.zip
cp -R mot-lilygo-full-lewisxhe-a76xx-transport/. .
rm -rf mot-lilygo-full-lewisxhe-a76xx-transport

python3 tools/apply_lilygo_full_lewisxhe_a76xx_transport.py
```

## Build and flash

```bash
cd firmware/lilygo-t-a7670
pio run -t clean
pio run
pio run -t upload
pio run -t uploadfs
```

## Test

Use the DNS MQTT hostname again.

```text
/api/lilygo/modem
/api/lilygo/mqtt
/api/lilygo/lte/mqtt-trace
/api/lilygo/lte/rx-debug
```
