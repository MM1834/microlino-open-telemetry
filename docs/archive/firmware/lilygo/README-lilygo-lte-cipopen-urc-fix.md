# MOT LilyGO LTE CIPOPEN URC Fix

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-cipopen-urc-fix.zip
cp -R mot-lilygo-lte-cipopen-urc-fix/. .
rm -rf mot-lilygo-lte-cipopen-urc-fix

python3 tools/apply_lilygo_lte_cipopen_urc_fix.py
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
/api/lilygo/mqtt
```

Look for `writeCalls > 0` in the trace. That means PubSubClient reached the MQTT CONNECT write step.
