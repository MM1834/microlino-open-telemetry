# LilyGO CAN via SN65HVD230

Pin plan:

```text
SN65HVD230 R/RX -> LilyGO GPIO32
SN65HVD230 D/TX -> LilyGO GPIO13
SN65HVD230 VCC  -> 3.3V
SN65HVD230 GND  -> GND
CANH            -> Microlino CAN-H
CANL            -> Microlino CAN-L
```

GPIO33 is not used because it is `MODEM_RI_PIN`.

Check whether the SN65HVD230 module has a fixed 120-ohm termination resistor before attaching to an existing vehicle/display CAN bus.
