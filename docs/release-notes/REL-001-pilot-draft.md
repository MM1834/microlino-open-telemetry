# REL-001 Portal Pilot Release Notes — Draft

> **Status:** Draft; release not yet approved
>
> **Audience:** Pilot user, support and release reviewer

## Included

- hosted Cognito login/logout with per-user vehicle isolation;
- controlled administrator invitations and expiring single-use vehicle claims;
- support for adding another claimed vehicle to an existing account;
- WROOM AWS firmware for hardware with or without the optional GPS module;
- LilyGO AWS firmware with WiFi preference and validated LTE/TLS fallback;
- authenticated local device administration, protected operational AP and local
  OTA disabled by default;
- portal distinction between WebSocket connectivity and telemetry freshness.

## Pilot operating boundary

- Accounts are invited by the maintainer; public self-registration is not enabled.
- Each physical adapter has a unique AWS IoT Thing/certificate identity.
- Device credentials are provisioned by the maintainer and are never given to a
  pilot user through the portal.
- Local USB recovery remains the authoritative recovery path.
- The pilot portal remains under `/motbeta/` until release promotion is approved.

## Known limitations

- automated device replacement, ownership transfer, loss recovery and certificate
  rotation are not implemented;
- cloud-managed OTA is not implemented;
- ABRP remains WiFi-only on LilyGO;
- LilyGO LTE has passed a functional vehicle test but not long-duration,
  weak-signal or adverse-power qualification;
- telemetry history is not a durable cloud history service;
- notification on charging/SOC thresholds is planned, not included;
- public account self-registration is deferred;
- OAuth callback `code` and `state` query parameters are currently retained in two
  restricted hosting access logs. The maintainer accepted this only for the
  invitation-only REL-001 pilot while provider remediation remains pending.

## Validation summary

- two-user REST/WebSocket isolation and live revoke/restore passed;
- hosted normal-user and administrator login/logout passed;
- expiring, atomic, single-use claim issuance and consumption passed;
- a fresh no-GPS WROOM was erased, uniquely provisioned and claimed end to end;
- an existing user successfully added a second vehicle claim;
- LilyGO connected to AWS IoT over LTE/TLS with WiFi absent and delivered live
  vehicle telemetry to the portal;
- repository tests and both AWS firmware builds passed before this draft update.

## Release decision still required

The final release record must name the exact commit, artifact hashes, approved
device list, support owner and rollback procedure. CLOUD-017 must be reviewed again
before public self-registration or a general public release.
