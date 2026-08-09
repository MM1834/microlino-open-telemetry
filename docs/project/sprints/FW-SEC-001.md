# FW-SEC-001 — Local Firmware Administration Hardening

> **Status:** Completed — implemented and physically validated on WROOM and LilyGO
>
> **Audience:** Firmware maintainer, device provisioner and pilot support
>
> **Started:** 2026-08-03
>
> **Completed:** 2026-08-09

## Objective

Prevent a nearby or same-network party from changing configuration, exporting
local secrets, resetting the device or installing arbitrary firmware on a device
issued to a pilot user. Apply the contract to ESP32-WROOM first and then port the
same boundary to LilyGO without coupling it to portal authentication.

## Security boundary

The local firmware UI remains a device-local setup, diagnostics, recovery and OTA
surface. Portal accounts do not authenticate the local UI. Each provisioned device
has one unique local administrator password known to its owner/support operator.

For the controlled pilot, WiFi encryption plus authenticated HTTP is accepted for
the local interface. HTTPS on the ESP32, signed firmware, Secure Boot and flash
encryption remain later hardening opportunities. Physical USB access remains a
trusted recovery boundary.

## ESP32-WROOM contract

### Controlled first setup

After a complete flash and partition erase, the device may expose a temporary open
setup AP only in the controlled provisioning environment. While no valid local
administrator password exists:

- only the minimum setup page and save action are available;
- OTA, configuration export/import, factory reset and operational APIs fail closed;
- onboarding cannot be marked complete;
- the administrator password must be 12–63 characters.

The device must not be issued to another person in this state.

### Provisioned operation

Once a valid password is stored:

- every fallback AP uses WPA2 with that password;
- the complete local WebUI and all local API routes require authentication;
- configuration forms never echo stored passwords or tokens into HTML;
- blank secret fields preserve their existing values;
- explicit secret replacement remains possible after authentication;
- configuration export excludes secrets by default;
- OTA requires both explicit enablement and successful authentication;
- factory reset returns to the controlled first-setup state;
- normal station-mode operation does not keep an AP active.

## LilyGO boundary

The same authentication, secret-handling and OTA contract applies. The LilyGO
retains its recovery AP during normal WiFi/LTE operation because it is the local
support path when no station network is reachable, but that operational AP is
WPA2-protected with the unique local administrator password. Only the controlled
first-setup state may be open.

## Acceptance gates

- [x] Missing/short admin password cannot authorize operational routes in code tests.
- [x] Provisioned fallback AP is never started without WPA2 in the implemented branch.
- [x] Unauthenticated config, export/import, reset and OTA requests are denied in code tests.
- [x] OTA is denied when disabled even with valid authentication in code tests.
- [x] Password and token values are not rendered into configuration HTML.
- [x] Blank secret fields preserve stored values.
- [x] Non-secret configuration export remains available only after authentication.
- [x] ESP32-WROOM AWS environment builds successfully.
- [x] Clean-flash first setup and authenticated fallback recovery pass on WROOM hardware.
- [x] Equivalent LilyGO authentication/AP/OTA gates pass on hardware.

## Stop conditions

- No flash erase, firmware upload, filesystem upload or credential mutation before
  the exact device and command sequence receive explicit approval.
- No shared default password and no credential committed to Git.
- No external pilot handoff with an open AP or unauthenticated OTA/admin surface.
- No claim that physical extraction is prevented without flash encryption.

## Closure decision

All repository and physical acceptance gates are satisfied for the defined WROOM
and LilyGO boundary. The package is closed. Secure Boot, flash encryption, signed
fleet rollback and other stronger physical-security controls remain separate
backlog opportunities rather than incomplete FW-SEC-001 scope. ESP32-C6 local
administration and recovery are covered by the completed C6-PH-001 successor
package.

## Related records

- [v1.0.0-rc.1](V1.0.0-RC.1.md)
- [Firmware known gaps](../../firmware/known-gaps.md)
- [Firmware OTA](../../firmware/ota.md)
- [Cloud credential handling](../../security/aws-iot-credentials.md)
