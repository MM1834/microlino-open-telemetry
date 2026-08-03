# MOT LilyGO TinyGSM LTE Client

## Apply

```bash
unzip ~/Downloads/mot-lilygo-tinygsm-lte-client.zip
cp -R mot-lilygo-tinygsm-lte-client/. .
rm -rf mot-lilygo-tinygsm-lte-client

python3 tools/apply_lilygo_tinygsm_lte_client.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

Set MQTT host to the broker IP first, e.g.

```text
84.73.108.227
```

Then check:

```text
/api/lilygo/lte/mqtt-trace
/api/lilygo/mqtt
```
