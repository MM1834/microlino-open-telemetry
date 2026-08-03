# MOT v1.0.2 – Testfahrt Fixes

Einspielen:

```bash
unzip ~/Downloads/mot-v1.0.2-testfahrt-fixes.zip
cp -R mot-v1.0.2-testfahrt-fixes/. .
rm -rf mot-v1.0.2-testfahrt-fixes

python3 tools/apply_v1_0_2_testfahrt_fixes.py
git diff
cd firmware/esp32-wroom
pio run
```

Enthält:
- Speed-History bei Fahrt alle 5 Sekunden
- Map-Zoom von 14/15 auf 17
- erste Trennung Rekuperation in der Anzeige
- WebUI Refresh von 10s auf 2s, falls im Code vorhanden
- Testfahrt-Findings als Dokument
