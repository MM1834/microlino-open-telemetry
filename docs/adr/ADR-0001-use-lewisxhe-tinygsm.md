# ADR-0001: Use LewisXhe TinyGSM for A7670G LTE

## Status

Accepted

## Context

The custom A7670 AT stack reached LTE/GPRS and TCP but failed reliably at MQTT receive/CONNACK handling.

## Decision

Use the LewisXhe TinyGSM A76XXSSL fork, matching the working obd2mqtt implementation.

## Consequences

Positive:

- Proven with A7670G-LLSE
- MQTT over LTE works
- Less custom AT handling

Negative:

- Dependency on a fork
- Secure HTTPS behavior still requires validation
