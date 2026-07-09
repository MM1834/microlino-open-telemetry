# Power

Reliable power is especially important for the LilyGO LTE hardware.

## ESP32-WROOM and WeAct CAN485

WiFi-only ESP32 setups are usually stable from USB or a regulated 5 V source. Avoid weak USB hubs and poor cables.

## LilyGO T-A7670G

LTE modems can draw short current peaks during registration and transmission. Use:

- stable 5 V supply,
- short good-quality USB cable,
- charged battery if installed,
- connected LTE antenna.

## Possible power symptoms

- Power LED does not light reliably.
- Modem initialization occasionally fails.
- LTE registration is unstable.
- Device behaves differently from laptop USB vs wall adapter.
