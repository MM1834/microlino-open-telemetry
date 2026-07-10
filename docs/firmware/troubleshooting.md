# Firmware troubleshooting

## MQTT works through WiFi but not LTE

This isolates broker credentials, topic composition and high-level telemetry logic. Investigate the LilyGO modem/client receive path, modem concurrency and power.

## Broker logs the client as connected, firmware reports timeout

The broker has processed MQTT `CONNECT`, but the firmware did not complete/observe `CONNACK`. Capture the broker-side packet flow and inspect the LTE client's `available()`/`read()` path.

## MQTT `connect failed`

Check DNS, TCP reachability, port forwarding and broker availability. A temporary `connect failed` after a broker restart may become a timeout once the listener is available again.

## No CAN frames

Check:

- vehicle awake state,
- CAN-H and CAN-L,
- common ground,
- 500 kbit/s configuration,
- GPIO32/GPIO13 mapping.

## GPS seen but not valid

The serial receiver is active but has no valid fix. Move the antenna to a sky-facing position and wait.

## Modem does not answer AT after reset/OTA

The documented recovery sequence is:

1. normal AT wait,
2. UART reinitialization,
3. soft modem/network cleanup,
4. power-key pulse,
5. report failure if still unavailable.

## WebUI is slow on LTE

Look for long blocking MQTT connects, HTTPS requests or repeated GPRS reconnects in the main execution path.

## Watchdog names an MQTT task

A task is blocking CPU0 without yielding sufficiently. Roll back the experimental task implementation and redesign modem ownership/concurrency before field use.

## Suspected power problem

Use a stable 5 V / 2 A supply, short USB cable, connected antenna and charged battery. Intermittent LEDs or modem startup differences between laptop USB and a wall adapter justify a dedicated power test.
