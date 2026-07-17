# GPS-1 package

This package intentionally starts with an isolated ESP32-WROOM GNSS test
firmware instead of immediately changing the production AWS build.

It also introduces the definitive shared `MotGps` parsing library and migrates
the LilyGO compatibility wrapper to it.

Start:

```bash
./tools/apply_gps_1_shared_library.py
cd firmware/esp32-wroom
pio run -e esp32dev-gps-test
pio run -e esp32dev-gps-test -t upload
pio device monitor
```
