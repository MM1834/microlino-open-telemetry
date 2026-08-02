# LilyGO LTE/GPRS

> **Status:** Code present; current beta readiness unverified
>
> **Audience:** Firmware developer and maintainer

The LilyGO target contains SIMCom A7670 modem initialization, registration/GPRS
management, TCP client diagnostics and a legacy MQTT transport candidate.

## Current build-path distinction

- `T-A7670X-AWS` uses `MotAwsIot` only when WiFi is connected. Its runtime reports
  transport `WiFi`; it does not send AWS MQTT/TLS through LTE.
- `lilygo-t-a7670` contains legacy MQTT transport selection preferring WiFi and
  falling back to an LTE client when GPRS is connected.

The second environment is legacy build structure, and historical experiments do
not establish that MQTT receive/CONNACK, reconnect, watchdog and long-running TLS
are reliable on the current commit.

## Source configuration

The board header identifies an A7670G modem and declares UART/control pins plus a
Swisscom APN default. Runtime configuration can supply the LTE APN. Provider
defaults must not be treated as portable beta configuration.

## Required validation before beta use

- modem boot and recovery;
- SIM registration and PDP/GPRS lifecycle;
- DNS and bidirectional TCP;
- MQTT/TLS send and receive;
- UTC acquisition for TLS;
- reconnect/backoff and WiFi transition;
- watchdog/concurrency behaviour;
- weak-signal and power scenarios;
- local WebUI responsiveness during modem failure.

## Historical investigations

Detailed experiments are indexed under [LTE development](../developer/lte/README.md).
They are engineering evidence, not current operational instructions.
