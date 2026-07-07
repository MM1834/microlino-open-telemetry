# MOT LilyGO LTE Debug + Backup Fix

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-debug-and-backup-fix.zip
cp -R mot-lilygo-lte-debug-and-backup-fix/. .
rm -rf mot-lilygo-lte-debug-and-backup-fix

python3 tools/apply_lilygo_lte_debug_and_backup_fix.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/lte/debug
/api/lilygo/mqtt
/api/config/export
```
