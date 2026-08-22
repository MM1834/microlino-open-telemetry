# CAN receive-only hardware design

> **Status:** Recommended safety architecture for new pilot and future custom hardware
>
> **Audience:** Hardware designer, pilot builder and security reviewer

MOT currently only receives and decodes vehicle CAN traffic. CAN transmission is
not required for telemetry, local diagnostics, AWS, ABRP or GPS. The firmware
therefore configures every supported TWAI controller as listen-only and contains
no application transmit path.

Firmware listen-only is not sufficient protection against an unauthorized image.
If ESP TX remains wired to transceiver TXD, replacement firmware could select
normal mode or bit-bang that GPIO. A physical default-off connection makes the
receive-only property independent of firmware.

## Recommended circuit per CAN channel

```text
Vehicle CAN-H/L <--> 3.3 V CAN transceiver
                         RXD ------------------> ESP CAN_RX
                         TXD ----+----[ open jumper ]---- ESP CAN_TX
                                 |
                              10 kOhm
                                 |
                                3V3
```

- Connect transceiver RXD directly to the assigned ESP receive GPIO.
- Hold transceiver TXD at the recessive-high level with approximately 10 kΩ to
  the transceiver's 3.3 V logic supply. Confirm the exact TXD thresholds and
  power-off behaviour from the selected transceiver datasheet.
- Route ESP CAN_TX through a normally open solder bridge, removable link or
  service jumper labelled `CAN TX ENABLE`.
- Use one independent jumper for CAN1 and one for CAN2.
- Ship and operate pilot devices with both jumpers open.
- Do not rely on a floating TXD input or on firmware GPIO state.

With the jumper open, the transceiver cannot be commanded dominant by ESP
firmware. The receiver path remains available, while the adapter contributes no
ACK or active error bits. Closing the jumper deliberately restores future TX
capability without redesigning the PCB, but must only occur with an approved use
case, firmware and vehicle integration procedure.

This interlock complements rather than replaces authenticated OTA, signed
firmware, Secure Boot, flash encryption and physical access controls. Those
controls restrict which software can run; the open TX link limits what even
unexpected software can do to the vehicle CAN bus.

## Existing pilot adapters

For already assembled adapters, the strongest retrofit is to disconnect each
ESP-to-transceiver TXD wire and add the pull-up at the transceiver input. Merely
removing the wire without verifying a defined recessive TXD level is not an
acceptable final state. Record the retrofit for CAN1 and CAN2 independently.

Termination remains a separate issue: a short diagnostic stub normally must not
add another 120 Ω termination to an already terminated vehicle bus.

## Related documents

- [CAN firmware and decoder pipeline](../firmware/can.md)
- [MOT ESP32-C6 Gen.2 pilot assembly](mot-esp32-c6-gen2-wiring.md)
- [Hardware overview](overview.md)
