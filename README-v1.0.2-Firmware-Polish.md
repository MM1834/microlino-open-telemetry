# MOT v1.0.2 – Firmware Polish Sprint

Dieses Paket ergänzt:

- stabile Device-Namen / MQTT Client-ID
- MQTT optional
- ABRP optional
- Configuration Export / Import

## Einspielen

```bash
unzip ~/Downloads/mot-v1.0.2-firmware-polish-sprint.zip
cp -R mot-v1.0.2-firmware-polish-sprint/. .
rm -rf mot-v1.0.2-firmware-polish-sprint

python3 tools/apply_v1_0_2_firmware_polish.py
pio run
```

## Test

1. WebConfig öffnen.
2. Device name setzen.
3. MQTT Host leer lassen → keine MQTT-Fehler.
4. MQTT Host setzen → stabile Client-ID im Broker prüfen.
5. `/api/config/export` testen.
6. Export JSON wieder importieren.
7. ABRP API key + token leer lassen → ABRP disabled.
8. ABRP API key + token setzen → ABRP enabled.
