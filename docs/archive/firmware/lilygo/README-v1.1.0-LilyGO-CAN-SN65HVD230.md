# MOT v1.1.0 – LilyGO CAN SN65HVD230

## Einspielen

```bash
unzip ~/Downloads/mot-v1.1.0-lilygo-can-sn65hvd230.zip
cp -R mot-v1.1.0-lilygo-can-sn65hvd230/. .
rm -rf mot-v1.1.0-lilygo-can-sn65hvd230

python3 tools/apply_v1_1_0_lilygo_can_sn65hvd230.py
```

## Pinout

```text
SN65HVD230 R/RX -> GPIO32
SN65HVD230 D/TX -> GPIO13
VCC             -> 3.3V
GND             -> GND
```

## Build / Upload

```bash
cd firmware/lilygo-t-a7670
pio run
pio run -t upload
```

## Test

```text
http://192.168.4.1/api/lilygo/can
http://192.168.4.1/api/lilygo/can/frames
```
