# SPR-0005 Device Field Validation

> **Result:** Passed for the exercised WROOM and LilyGO pilot paths
>
> **Date:** 2026-08-03
>
> **Firmware implementation commit:** `e2bdd17`

## Boundary

This record consolidates the controlled physical-device evidence produced after
ONB-001.B2. It does not approve public self-registration, cloud OTA, automated
device replacement or general LilyGO production readiness.

## ESP32-WROOM without GPS

- a previously unused adapter was fully erased, flashed with the AWS build and
  provisioned with its own `beta-02` Thing/certificate material;
- firmware correctly reported that no GPS module was detected rather than treating
  UART noise as valid GPS data;
- the operational AP required the configured password and local OTA was disabled;
- an existing portal user claimed `beta-02` as an additional vehicle without
  losing the existing assignment;
- the hosted portal exposed only authorized vehicles and rejected an already
  assigned vehicle claim.

## LilyGO T-A7670G

- the AWS build compiled and was flashed without erasing the existing unique
  device identity or LittleFS credentials;
- the setup AP required the local administrator password and operational WebUI/API
  access required HTTP authentication;
- local OTA defaulted to disabled and normal configuration export omitted secrets;
- with the configured WiFi hotspot unavailable, the modem registered, established
  GPRS and received a mobile IP address;
- the firmware installed the AWS root CA, device certificate and private key into
  the modem TLS client without logging their contents;
- AWS IoT connected over LTE/TLS using the device Thing identity;
- periodic attempts to return to preferred WiFi did not cause an observed AWS MQTT
  disconnect during the validation window;
- connected to the Microlino CAN bus, current telemetry reached the hosted portal
  and changed from stale to current without a browser reload;
- GPS updates remained independent of CAN freshness;
- ABRP and legacy MQTT were disabled for the final validated device configuration.

## Automated and compile evidence

- 72 repository Python tests passed after the LilyGO security/LTE changes;
- `T-A7670X-AWS` built successfully with 53,992 bytes RAM use and 1,146,613 bytes
  program-flash use (87.5% of the application partition); the resulting
  `firmware.bin` SHA-256 was
  `7be0e9ccd8454c4a675a2cee5237da4bbe6b3be87356a7deb826931be6fee0d0`;
- the shared AWS client change was regression-built successfully with
  `esp32dev-aws`, using 50,864 bytes RAM and 1,147,329 bytes program flash (87.5%);
  the resulting `firmware.bin` SHA-256 was
  `b839ffd65ee20bbd803216e362727cf58b8d27197c1c63926e648b6bcae82ecb`;
- the exact release-candidate build and artifact hashes remain a REL-001 gate
  because documentation changes follow implementation commit `e2bdd17`.

## Remaining risks

- LilyGO flash margin is approximately 12.5%; future features must measure it;
- the observed LTE test is functional evidence, not a long-duration soak or power
  qualification;
- ABRP does not yet use the shared LTE/TLS transport;
- OAuth callback query parameters remain present in hosted access logs and are
  covered only by the bounded REL-001 pilot acceptance;
- device replacement, loss, certificate rotation and ownership transfer remain
  controlled maintainer workflows or unimplemented B3 scope.

## Related documents

- [v1.0.0-rc.1](../project/sprints/V1.0.0-RC.1.md)
- [ONB-001.B2 validation](ONB-001-B2-validation.md)
- [WROOM build evidence](SPR-0005-wroom-aws-build.md)
- [Cloud risk register](../security/cloud-risk-register.md)
