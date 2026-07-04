# MOT v1.0.2 – Location Map Cleanup

Dieses Paket entfernt bzw. versteckt die alte synthetische Standortgrafik, damit nur noch die OpenStreetMap-Karte sichtbar ist.

## Einspielen

```bash
unzip ~/Downloads/mot-v1.0.2-location-map-cleanup.zip
cp -R mot-v1.0.2-location-map-cleanup/. .
rm -rf mot-v1.0.2-location-map-cleanup

python3 tools/apply_v1_0_2_location_map_cleanup.py
git diff
```

Danach Dashboard neu laden, ggf. Browser-Cache leeren.
