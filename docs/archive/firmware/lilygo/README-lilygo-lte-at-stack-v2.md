# MOT LilyGO LTE AT Stack v2

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-at-stack-v2.zip
cp -R mot-lilygo-lte-at-stack-v2/. .
rm -rf mot-lilygo-lte-at-stack-v2

python3 tools/apply_lilygo_lte_at_stack_v2.py
```

Requires the URC RX buffer patch already applied.

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/mqtt
/api/lilygo/lte/rx-debug
```
