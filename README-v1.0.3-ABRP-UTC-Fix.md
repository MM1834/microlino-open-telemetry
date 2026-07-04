# MOT v1.0.3 – ABRP UTC/NTP Fix

Dieses Paket korrigiert den ABRP-Zeitstempel:

- `utc` wird nur gesendet, wenn die ESP32-Zeit gültig ist.
- NTP wird nach WLAN-Verbindung angestoßen, sofern die Netzwerkdatei automatisch erkannt wird.
- Es werden weiterhin keine `lat`/`lon` Werte gesendet, solange kein GPS/GNSS-Modul vorhanden ist.

## Einspielen

```bash
unzip ~/Downloads/mot-v1.0.3-abrp-ntp-utc-fix.zip
cp -R mot-v1.0.3-abrp-ntp-utc-fix/. .
rm -rf mot-v1.0.3-abrp-ntp-utc-fix

python3 tools/apply_v1_0_3_abrp_ntp_utc_fix.py
cd firmware/esp32-wroom
pio run
```

## Erwartung

Vor NTP:

```json
{"soc":54.0,"speed":0.0,"power":9.00,"is_charging":false}
```

Nach NTP:

```json
{"soc":54.0,"utc":1750000000,"speed":0.0,"power":9.00,"is_charging":false}
```
