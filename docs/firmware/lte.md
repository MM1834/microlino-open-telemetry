# LilyGO LTE/GPRS

> **Status:** AWS IoT LTE/TLS functionally field-validated; extended qualification open
>
> **Audience:** Firmware developer and maintainer

The LilyGO target contains SIMCom A7670 modem initialization, registration/GPRS
management, TCP client diagnostics and an AWS IoT X.509 transport.

## Current build-path distinction

- `T-A7670X-AWS` prefers WiFi and falls back to the A7670 secure client when GPRS
  is connected. It uploads the per-device CA/certificate/private-key material to
  the modem certificate store and reports the active `WiFi` or `LTE` transport.
- `lilygo-t-a7670` contains legacy MQTT transport selection preferring WiFi and
  falling back to an LTE client when GPRS is connected.

The second environment is legacy build structure. On 2026-08-03 the AWS environment
connected to AWS IoT over LTE/TLS with WiFi absent, tolerated periodic attempts to
return to WiFi and delivered current CAN/GPS telemetry through the hosted portal.
That is functional field evidence, not long-duration or adverse-condition
qualification.

## Runtime configuration

The board header identifies an A7670G modem and declares UART/control pins. LTE is
inactive until an APN is configured in the authenticated local WebUI or imported
configuration. Optional APN username/password values are supported; stored
passwords are not rendered by the UI. New and reset devices have no provider
default.

Normal reconnect attempts wait at most 15 seconds for registration and use
exponential retry backoff from 15 seconds to 5 minutes. Four consecutive failures
trigger a modem power/init recovery. Diagnostics expose the current interval and
recovery counters. These bounds improve loop availability but do not make the
underlying TinyGSM registration call asynchronous.

## Remaining qualification

- modem boot and recovery;
- SIM registration and PDP/GPRS lifecycle;
- DNS and bidirectional TCP;
- long-duration MQTT/TLS reconnect and session soak;
- UTC fallback when neither valid GPS time nor WiFi NTP is available;
- repeated WiFi/LTE transitions and loss of mobile registration;
- watchdog/concurrency behaviour;
- weak-signal and power scenarios;
- local WebUI responsiveness during modem failure.

ABRP HTTPS remains WiFi-only. Enabling ABRP does not route it over LTE.

## Historical investigations

Detailed experiments remain available in Git history. They are engineering
evidence, not current operational instructions.
