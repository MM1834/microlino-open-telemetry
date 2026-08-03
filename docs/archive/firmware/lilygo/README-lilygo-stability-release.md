# MOT LilyGO Stability Release

## Apply from repository root

```bash
cd /Users/martin/Documents/MICROLINO/microlino-open-telemetry

unzip ~/Downloads/mot-lilygo-stability-release.zip
cp -R mot-lilygo-stability-release/. .
rm -rf mot-lilygo-stability-release

python3 tools/apply_lilygo_stability_release_cleanup.py
```

## Build and flash

```bash
cd firmware/lilygo-t-a7670
pio run -t clean
pio run
pio run -t upload
pio run -t uploadfs
```

## Tag

```bash
git status
git add .
git commit -m "release: stabilize LilyGO LTE MQTT integration"
git tag -a v1.1.0-lilygo-stability -m "v1.1.0 LilyGO stability release"
```
