# MOT LilyGO LTE CIPOPEN Hybrid Fix

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-cipopen-hybrid-fix.zip
cp -R mot-lilygo-lte-cipopen-hybrid-fix/. .
rm -rf mot-lilygo-lte-cipopen-hybrid-fix

python3 tools/apply_lilygo_lte_cipopen_hybrid_fix.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
POST /api/lilygo/lte/mqtt-trace/clear
GET  /api/lilygo/lte/mqtt-trace
GET  /api/lilygo/mqtt
```

Expected signal: `writeCalls > 0`.
