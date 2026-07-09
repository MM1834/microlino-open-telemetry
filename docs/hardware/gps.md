# GPS

The LilyGO platform uses an external L76K GPS module.

![LilyGO with L76K GPS](../assets/images/hardware/lilygo-t-a7670-board-top-incl-l76k-gps-module.png)

## Pins

| Function | GPIO |
|---|---:|
| GPS RX | 22 |
| GPS TX | 21 |
| PPS | 23 |
| WAKEUP | 19 |

## Installation tips

- Place the GPS antenna with sky visibility.
- Keep GPS and LTE antennas separated.
- Wait for a valid fix before relying on location.
- First fix after power loss can take longer.
