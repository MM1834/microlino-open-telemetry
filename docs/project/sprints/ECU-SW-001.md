# ECU-SW-001 — passive vehicle software versions

> **Status:** Repository-ready; physical end-to-end acceptance pending
>
> **Safety boundary:** Receive-only CAN. No OBD, UDS, PID or other request.

## Objective

Decode the three cyclic Pioneer software-version broadcasts and expose them in
the local adapter UI, AWS telemetry, local MQTT, the responsive dashboard home
page and a dedicated MOT Display screen.

## Signal contract

| Component | CAN source | Layout | Current evidence |
|---|---|---|---|
| VCU | Standard-CAN `0x700` and Display-CAN `0x606` | minor byte 0, major byte 1, little-endian build bytes 4–5 | Confirmed `7.23.1885` against vehicle display |
| IPC | Display-CAN `0x604` | major/minor/patch bytes 5–7 | Strong candidate `1.3.0`; raw sequence stable during B.025 capture |
| BDU | Display-CAN `0x0AC` | major/minor/patch bytes 0–2 | Strong candidate `1.1.0`; complete payload matches tuple |

Canonical telemetry suffixes are `vehicle/vcu_software_version`,
`vehicle/ipc_software_version` and `vehicle/bdu_software_version`. Values are
semantic-version strings. Missing frames produce no value; firmware does not
invent defaults.

## Implemented surfaces

- shared Standard-CAN Pioneer and Display-CAN decoders;
- authenticated local adapter status page and `/api/status`;
- retained `telemetry/state/v1` envelopes for AWS and local MQTT;
- AWS ingest allowlist and Vehicle API/dashboard state path;
- responsive dashboard home-page card for desktop and smartphone;
- additive encrypted BLE `SoftwareVersionsV1` record;
- dedicated `Software-Versionen` pages on the 1.28-inch C6, DualEye 1.28-inch
  and 1.85-inch touch MOT Display targets.

IPC and BDU remain marked as candidates in engineering evidence and in the BLE
flags until independently compared with the vehicle version page or a second
software level. The UI shows the received values without claiming hardware
revision semantics.

## Explicit exclusion

Standard-CAN `0x105` temperatures are not included. The powered-vehicle repeat
capture received more than 28,000 error-free Standard-CAN frames and zero
`0x105` frames, so those temperatures remain unconfirmed/not detected.

## Acceptance

1. Adapter and all three display targets compile within flash/RAM limits.
2. Decoder contract tests cover all three IDs and byte layouts.
3. AWS accepts all three string suffixes and the dashboard renders them after a
   vehicle change without retaining the previous vehicle's values.
4. On B.025, local UI, local MQTT, AWS/dashboard and the display show VCU
   `7.23.1885`, IPC `1.3.0` and BDU `1.1.0` from passive traffic.
5. CAN transmit queues remain disabled and no diagnostic request is introduced.
