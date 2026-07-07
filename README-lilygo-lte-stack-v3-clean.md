# MOT LilyGO LTE Stack v3 Clean

## Apply from repository root

```bash
cd /Users/martin/Documents/MICROLINO/microlino-open-telemetry

unzip ~/Downloads/mot-lilygo-lte-stack-v3-clean.zip
cp -R mot-lilygo-lte-stack-v3-clean/. .
rm -rf mot-lilygo-lte-stack-v3-clean

python3 tools/apply_lilygo_lte_stack_v3_clean.py
```

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
/api/lilygo/lte/debug
```
