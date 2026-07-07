# MOT LilyGO LTE RX Debug

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-rx-debug.zip
cp -R mot-lilygo-lte-rx-debug/. .
rm -rf mot-lilygo-lte-rx-debug

python3 tools/apply_lilygo_lte_rx_debug.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

After MQTT LTE reaches timeout:

```text
/api/lilygo/lte/rx-debug
/api/lilygo/mqtt
```
