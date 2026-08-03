# MOT LilyGO LTE TCP Test Endpoint

## Apply

```bash
unzip ~/Downloads/mot-lilygo-lte-tcp-test-endpoint.zip
cp -R mot-lilygo-lte-tcp-test-endpoint/. .
rm -rf mot-lilygo-lte-tcp-test-endpoint

python3 tools/apply_lilygo_lte_tcp_test_endpoint.py
```

## Build

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
/api/lilygo/lte/tcp-test
/api/lilygo/lte/tcp-test?host=mmds.muehlberg.ch&port=22025
```
