# Active Work Package Index

> **Status:** Current index
>
> **Audience:** Maintainer and auditor

## Current release record

- [v1.0.0-rc.1 — completed repository and documentation consolidation](V1.0.0-RC.1.md)

## Active implementation work

- [SES-001 — Cognito domain email delivery](SES-001.md)
- [C6-ENV-001 — unified C6 build environments](C6-ENV-001.md)
- [HIS-001 — bounded telemetry history pilot](HIS-001.md)
- [HIS-BIN-001 — combined binary charging History](HIS-BIN-001.md)
- [HIS-SIGN-001 — signed net-power History presentation](HIS-SIGN-001.md)
- [JNY-001 — journey summary and energy email pilot](JNY-001.md)
- [SPR-0005 — ESP32-WROOM beta readiness and portal onboarding](SPR-0005.md)
- [ONB-001.B — Controlled User and Device Onboarding](ONB-001-B.md)
- [LG-CAN2-001 — LilyGO mobile dual-CAN pilot](LG-CAN2-001.md)

## Completed predecessors

- [ONB-UX-001](ONB-UX-001.md) completed the guided C6 local onboarding flow,
  physical factory-first-run acceptance and configuration-preserving rollout to
  B021, B023 and B024 on 2026-08-25.
- [PWR-FRESH-001](PWR-FRESH-001.md) marks stale charging/power overview values
  and net-power History measurements without hiding retained data; hosted desktop
  and smartphone acceptance passed on 2026-08-24.
- [NTF-002](NTF-002.md) completed the qualified charging-stop email path with
  independent per-vehicle SOC target, durable 45+60-second validation and
  physical `pioneer` acceptance on 2026-08-24; SMS remains deferred.
- [CACHE-001](CACHE-001.md) delivered the optional bounded SOC/Speed offline
  cache, isolated and production Backfill paths, physical N16 power/replay
  acceptance and GPS-derived UTC cold-start proof on 2026-08-21.
- [DEMO-001](DEMO-001.md) deployed the isolated frozen demo dataset, enforced
  server-side read-only notification and onboarding boundaries, and passed hosted
  acceptance on 2026-08-20.
- [NTF-FIX-001](NTF-FIX-001.md) reconciled stale stored email-confirmation state
  with authoritative SNS subscription state and passed hosted portal acceptance
  on 2026-08-20.
- [AUTH-PERSIST-001](AUTH-PERSIST-001.md) completed the optional bounded
  trusted-device portal login and passed hosted desktop/smartphone acceptance on
  2026-08-14 without changing the default session-only path.
- [C6-SVC-001](C6-SVC-001.md) completed shared ABRP and authenticated local
  onboarding parity on C6, accepted the N16-AWS hardware path and retained XIAO
  as an unflashed 4 MB compatibility build below the 85% OTA-slot gate.
- [WIFI-001](WIFI-001.md) completed preferred Home-WiFi, Mobile fallback,
  automatic Home return and protected-AP recovery on C6.
- [NTF-001](NTF-001.md) completed the bounded charging/SOC email notification
  and authenticated per-vehicle portal-settings pilot on 2026-08-08; SMS remains
  a deferred channel.
- [FW-SEC-001](FW-SEC-001.md) completed local administration hardening and
  physical WROOM/LilyGO validation on 2026-08-09.
- [C6-PH-001](C6-PH-001.md) completed N16 production hardening and XIAO
  compatibility validation on 2026-08-07.
- [C6-001](C6-001.md) closed the bounded N16 dual-CAN WiFi/AWS pilot
  qualification on 2026-08-06.
- [WEB-001](WEB-001.md) delivered the repository-owned public landing page and
  completed hosted desktop/smartphone acceptance on 2026-08-05.
- REL-001 established and validated the accepted portal pilot baseline now carried
  by `v1.0.0-rc.1`.
- DOC-001 established the source-based documentation baseline.

Completed delivery records are available through Git history rather than retained
as parallel status documents in the working tree.

## Parked work

- [LTE-001](LTE-001.md) retains the functionally validated LilyGO A7670 baseline;
  extended production qualification is parked while feature work focuses on C6.
