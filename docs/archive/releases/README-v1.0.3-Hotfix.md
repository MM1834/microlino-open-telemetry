# MOT v1.0.3 Hotfix

## Einspielen

```bash
unzip ~/Downloads/mot-v1.0.3-hotfix.zip
cp -R mot-v1.0.3-hotfix/. .
rm -rf mot-v1.0.3-hotfix

python3 tools/apply_v1_0_3_hotfix.py
cd firmware/esp32-wroom
pio run
pio run -t upload
```

## Test

- Boot-Log/WebUI zeigen nicht mehr `0.9.x-dev`.
- Factory Reset fragt nach Bestätigung.
- Ohne MQTT Host erscheinen keine leeren DNS-Fehler mehr.
- ABRP funktioniert weiterhin.

fix(dashboard): show active AWS Vehicle API provider instead of MQTT
