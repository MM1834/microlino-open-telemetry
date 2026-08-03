# MOT LilyGO Modem Boot Recovery

## Apply

```bash
unzip ~/Downloads/mot-lilygo-modem-boot-recovery.zip
cp -R mot-lilygo-modem-boot-recovery/. .
rm -rf mot-lilygo-modem-boot-recovery

python3 tools/apply_lilygo_modem_boot_recovery.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```
