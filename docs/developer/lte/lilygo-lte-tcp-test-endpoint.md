> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO LTE TCP Test Endpoint

Adds an isolated LTE TCP test endpoint:

```text
GET  /api/lilygo/lte/tcp-test
POST /api/lilygo/lte/tcp-test
```

By default it uses the configured MQTT host and port.

Optional parameters:

```text
/api/lilygo/lte/tcp-test?host=mmds.muehlberg.ch&port=22025
```

The endpoint performs:

```text
AT+CIPCLOSE=0
AT+NETOPEN
AT+NETOPEN?
AT+CIPOPEN=0,"TCP","host",port
AT+CIPSTATUS
AT+CIPCLOSE=0
```

It returns the raw AT responses as JSON and closes the socket afterwards.
