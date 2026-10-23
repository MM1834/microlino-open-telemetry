# Local devices

> **Status:** REV48 repository candidate
>
> **Audience:** Pilot user, support and firmware developer

The authenticated adapter WebUI owns local BLE-device assignment. MOT Cloud and
the portal are not required and cannot silently change an assignment.

## 12-V battery monitor

Open **Local devices**, close the BM2/BM6 smartphone application and start the
two-minute search window. The page lists only nearby advertisements with the
known FFF0 service or a BM/battery-monitor family name. Selecting a candidate
stores its normalized BLE address and detected `bm2` or `bm6` protocol in the
adapter's NVS. Normal acquisition then accepts only that address and validates
the real FFF0/FFF4 service, notification payload and plausible values before any
measurement becomes available.

Unassigning the monitor stops its connection, removes the persisted identity
and invalidates live/history and alarm state. Searching alone never changes the
active assignment. Every mutating route requires the local administrator and
the established same-origin check.

B.025 uses a one-time REV47-to-REV48 migration default for the already accepted
AEG monitor. After that address has been written to NVS it is managed in exactly
the same way as every other local assignment. The universal production target
contains no preselected monitor.

## Shared slot model

The registry has five stable slots:

- 12-V board battery;
- tyre front left;
- tyre front right;
- tyre rear left;
- tyre rear right.

Only the board-battery slot is currently assignable. The four wheel slots are
reserved so a qualified TPMS implementation can reuse the same local ownership,
storage and unassignment rules. They remain disabled until the real sensors'
protocol, stable identity, wheel assignment and plausibility rules are known.

## Display boundary

REV48 does not yet expose device administration to MOT Display. The existing
encrypted display records remain unchanged. A later display increment can add a
separate encrypted management characteristic and explicit service gesture over
the same five slots; it must not overload telemetry characteristics or permit a
normal page gesture to change an assignment.
