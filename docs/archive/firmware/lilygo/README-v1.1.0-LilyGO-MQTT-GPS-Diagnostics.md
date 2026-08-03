# MOT v1.1.0 – LilyGO MQTT GPS + Diagnostics

## Einspielen

```bash
unzip ~/Downloads/mot-v1.1.0-lilygo-mqtt-gps-diagnostics.zip
cp -R mot-v1.1.0-lilygo-mqtt-gps-diagnostics/. .
rm -rf mot-v1.1.0-lilygo-mqtt-gps-diagnostics
```

## Build / Upload

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/mqtt
/api/lilygo/gps
```

MQTT `state=-1` bedeutet connection timeout. Dann Broker-IP/Port prüfen, insbesondere ob `192.168.11.70:2025` wirklich der MQTT Listener ist.
