# MOT v1.0.3 – ABRP Integration

## Einspielen

```bash
unzip ~/Downloads/mot-v1.0.3-abrp-integration.zip
cp -R mot-v1.0.3-abrp-integration/. .
rm -rf mot-v1.0.3-abrp-integration

python3 tools/apply_v1_0_3_abrp_integration.py
cd firmware/esp32-wroom
pio run
```

## Test

1. WebConfig öffnen.
2. ABRP API Key und User Token eintragen.
3. Speichern / Neustart.
4. **Test ABRP Send** ausführen.
5. `/api/abrp/status` prüfen.

## Sicherheit

API-Key und Token liegen im ESP32-Flash. Sie werden nicht per MQTT publiziert und nicht im Serial Log ausgegeben.
