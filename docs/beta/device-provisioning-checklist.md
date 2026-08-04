# ESP32-WROOM Beta Device Provisioning Checklist

> **Status:** Draft; execution requires separate build, AWS and hardware approval
>
> **Audience:** Authorized beta provisioner

## Device record

- [ ] Assign a physical asset/device label.
- [ ] Read and record the firmware-reported MOT device ID from the serial boot
  output or authenticated local status page. Do not derive it from the final three
  colon-separated bytes printed by esptool: `ESP.getEfuseMac()` uses a different
  byte representation for `motDeviceShortId()`.
- [ ] Assign one vehicle ID and human-readable vehicle name.
- [ ] Record hardware variant: GPS installed or not installed.
- [ ] Record board, transceiver, harness revision and enclosure revision.
- [ ] Record responsible beta user without placing personal data in public Git.

## Hardware review gate

- [ ] Confirm ESP32 CAN RX27/TX26 against the physical harness.
- [ ] Confirm transceiver voltage compatibility and common ground.
- [ ] Confirm termination strategy for the existing vehicle bus.
- [ ] Confirm passive connection and absence of application transmit behaviour.
- [ ] For GPS variant, confirm UART RX16/TX17, power and antenna placement.
- [ ] Inspect strain relief, insulation, enclosure and power supply.

No connection to a vehicle is authorized by this draft checklist alone.

## Firmware artifact gate

- [ ] Use the approved ESP32 AWS environment/artifact.
- [ ] Record Git commit, PlatformIO environment, build time and binary SHA-256.
- [ ] Confirm the firmware reports the expected `MOT_VERSION` and `esp32-wroom`.
- [ ] Preserve the approved binary in a controlled release location.

Build and flash steps remain outside DOC-001 until explicitly approved.

## AWS identity gate

- [ ] One Thing and one active certificate are assigned to this physical device.
- [ ] MQTT client ID equals the intended Thing name.
- [ ] Device metadata contains the correct endpoint, Thing name, vehicle ID and
  topic prefix.
- [ ] Policy scope is reviewed for the assigned device/topic namespace.
- [ ] Certificate ID/ownership record is stored outside public Git.
- [ ] No credential is shared with another beta device.

Creating or modifying AWS resources requires separate approval.

## Local security configuration

- [ ] During controlled first setup, configure a unique 12–63 character local
  admin password.
- [ ] Record it in the approved secret/support channel, not the handoff sheet.
- [ ] Confirm the operational fallback AP uses that password as its WPA2 key.
- [ ] Confirm unauthenticated operational WebUI/API access fails closed.
- [ ] Keep local OTA disabled unless an approved update is being performed.
- [ ] Confirm legacy MQTT is disabled unless explicitly required.
- [ ] Confirm ABRP is disabled unless provisioned for that tester.
- [ ] Confirm the user understands that only the controlled first-setup AP is open;
  after provisioning, fallback AP and WebUI require the local admin password.

## Functional validation gate

- [ ] Device boots and exposes the expected ID/version.
- [ ] Fallback AP naming and local access are confirmed.
- [ ] WiFi save/reboot and fallback recovery are confirmed.
- [ ] Display-CAN data is checked on the intended vehicle/interface.
- [ ] AWS X.509 connection and assigned topic ingestion are confirmed.
- [ ] No other vehicle appears for the test user once portal authorization exists.
- [ ] GPS-no-hardware behaviour matches the no-GPS variant.
- [ ] GPS-equipped variant detects NMEA and obtains a fix in suitable conditions.
- [ ] Backup, local OTA and recovery are tested with approved artifacts.
- [ ] Factory reset behaviour, including retained LittleFS identity, is understood.

## Handoff

- [ ] Give the tester the reviewed beta guide and support contact.
- [ ] Provide only their local setup/portal information.
- [ ] Record accepted known limitations and expiry/end of beta access.
- [ ] Confirm return, revocation and replacement procedure ownership.
