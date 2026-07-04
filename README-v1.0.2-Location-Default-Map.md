# MOT v1.0.2 – Default Location Map

Dieses Paket ersetzt die grobe synthetische Standortgrafik durch eine OpenStreetMap-Einbettung.

Die Karte nutzt automatisch:

1. live GPS/MQTT-Koordinaten, falls vorhanden
2. sonst `vehicle.defaultLocation` aus `config.js`

## Einspielen

```bash
unzip ~/Downloads/mot-v1.0.2-location-default-map.zip
cp -R mot-v1.0.2-location-default-map/. .
rm -rf mot-v1.0.2-location-default-map

python3 tools/apply_v1_0_2_location_default_map.py
git diff
```
