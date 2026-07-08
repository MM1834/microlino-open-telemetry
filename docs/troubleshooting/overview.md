# Troubleshooting

## MQTT rc=-2

Connection failed before MQTT session.

## MQTT rc=-4

Connection timeout. Check credentials, broker logs and CONNACK response.

## No LTE

Check SIM, APN, antenna, signal quality and modem recovery logs.

## Slow WebUI

Avoid long blocking modem operations in the main loop.
