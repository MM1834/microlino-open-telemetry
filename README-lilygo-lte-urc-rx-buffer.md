# MOT LilyGO LTE URC RX Buffer

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-urc-rx-buffer.zip
cp -R mot-lilygo-lte-urc-rx-buffer/. .
rm -rf mot-lilygo-lte-urc-rx-buffer

python3 tools/apply_lilygo_lte_urc_rx_buffer.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/lte/rx-debug
/api/lilygo/mqtt
```
