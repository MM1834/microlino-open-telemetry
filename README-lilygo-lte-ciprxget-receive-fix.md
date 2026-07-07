# MOT LilyGO LTE CIPRXGET Receive Fix

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-ciprxget-receive-fix.zip
cp -R mot-lilygo-lte-ciprxget-receive-fix/. .
rm -rf mot-lilygo-lte-ciprxget-receive-fix

python3 tools/apply_lilygo_lte_ciprxget_receive_fix.py
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

Expected next milestone:

```text
writeCalls > 0
lastAvailable > 0
readCalls > 0
```
