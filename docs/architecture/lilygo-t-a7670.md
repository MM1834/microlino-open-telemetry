# LilyGO T-A7670G R2 Architecture

The LilyGO target adds LTE and GPS capability to Microlino Open Telemetry.

## Hardware

Board:

```text
LilyGO T-A7670G R2 / T-A7670X-GPS V1.1
ESP32-WROVER
SIMCom A7670G-LLSE
External L76K GPS
```

## Confirmed pins

```text
MODEM_RX_PIN      GPIO27
MODEM_TX_PIN      GPIO26
MODEM_PWR_PIN     GPIO4
BOARD_POWER_ON    GPIO12
MODEM_RST_PIN     GPIO5
MODEM_DTR_PIN     GPIO25
MODEM_RI_PIN      GPIO33

GPS_RX_PIN        GPIO22
GPS_TX_PIN        GPIO21
GPS_PPS_PIN       GPIO23
GPS_WAKEUP_PIN    GPIO19

CAN_RX_PIN        GPIO32
CAN_TX_PIN        GPIO13
```

GPIO33 is reserved for `MODEM_RI_PIN` and is not used for CAN.

## Network priority

```text
WiFi available
  -> use WiFi

WiFi lost
  -> wait grace period
  -> fail over to LTE

WiFi returns
  -> switch back to WiFi
```

## Known issue

If the modem enters a bad state, a full power-cycle may be required. ESP32 reset alone may not recover the modem.
