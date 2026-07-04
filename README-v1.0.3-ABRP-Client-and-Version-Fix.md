# MOT v1.0.3 – ABRP Client + Firmware Version Fix

Dieses Paket korrigiert:

- ABRP sendet nicht mehr ohne gültige NTP-Zeit.
- `utc` ist bei ABRP wieder verpflichtend und immer gültig.
- Kein `lat/lon`, solange kein GPS/GNSS-Modul vorhanden ist.
- ABRP Status zeigt `timeValid` und `utc`.
- Firmware-Version `0.9.1-dev` wird auf `1.0.3-dev` aktualisiert, wo sie gefunden wird.

## Einspielen

```bash
unzip ~/Downloads/mot-v1.0.3-abrp-client-and-version-fix.zip
cp -R mot-v1.0.3-abrp-client-and-version-fix/. .
rm -rf mot-v1.0.3-abrp-client-and-version-fix

python3 tools/apply_v1_0_3_abrp_client_and_version_fix.py
cd firmware/esp32-wroom
pio run
```

## Erwartung

Vor gültiger NTP-Zeit:

```text
ABRP waiting for valid system time (NTP)
```

Nach NTP:

```json
{"soc":56.0,"utc":1750000000,"speed":0.0,"power":88.00,"is_charging":true}
```
