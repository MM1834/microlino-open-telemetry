# MOT v1.1.0 – LilyGO Sprint 3: WiFi first, else LTE

Netzwerklogik:

```text
WiFi verfügbar  -> WiFi
sonst LTE bereit -> LTE
sonst AP only
```

Der Setup-AP bleibt aktiv.

## Einspielen

```bash
unzip ~/Downloads/mot-v1.1.0-lilygo-sprint3-network-priority.zip
cp -R mot-v1.1.0-lilygo-sprint3-network-priority/. .
rm -rf mot-v1.1.0-lilygo-sprint3-network-priority

python3 tools/apply_v1_1_0_lilygo_sprint3_network_priority.py
```

## Optional WiFi

In `firmware/lilygo-t-a7670/platformio.ini` aktivieren:

```ini
-D WIFI_SSID=\"...\"
-D WIFI_PASS=\"...\"
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

API:

```text
/api/lilygo/network
```
