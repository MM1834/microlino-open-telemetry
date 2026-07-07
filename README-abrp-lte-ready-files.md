# MOT LilyGO ABRP LTE Ready Files

Copy these two files into your firmware tree:

```bash
cp src/abrp/lilygo_abrp.cpp firmware/lilygo-t-a7670/src/abrp/lilygo_abrp.cpp
cp src/abrp/lilygo_abrp.h   firmware/lilygo-t-a7670/src/abrp/lilygo_abrp.h
```

Then build and upload:

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

Test:

```text
POST /api/lilygo/abrp/test
GET  /api/lilygo/abrp
```

Note: WiFi uses HTTPS via HTTPClient. LTE uses plain HTTP over the LewisXhe TinyGSM Client because the MOT modem wrapper currently exposes a plain `Client*`.
