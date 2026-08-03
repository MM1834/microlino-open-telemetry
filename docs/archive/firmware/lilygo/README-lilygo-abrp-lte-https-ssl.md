# MOT LilyGO ABRP LTE HTTPS/SSL

## Apply from repository root

```bash
cd /Users/martin/Documents/MICROLINO/microlino-open-telemetry
unzip ~/Downloads/mot-lilygo-abrp-lte-https-ssl.zip
cp -R mot-lilygo-abrp-lte-https-ssl/. .
rm -rf mot-lilygo-abrp-lte-https-ssl

python3 tools/apply_lilygo_abrp_lte_https_ssl.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run -t clean
pio run
pio run -t upload
```
