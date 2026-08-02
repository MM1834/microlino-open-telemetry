# ESP32-WROOM Beta Release-readiness Checklist

> **Status:** Draft; no release is currently approved
>
> **Audience:** Maintainer and beta release reviewer

## Evidence

- [ ] Exact release commit and branch recorded.
- [ ] Reproducible build evidence exists for the intended AWS environment.
- [ ] Binary hashes and version strings agree.
- [ ] Hardware validation covers with-GPS and without-GPS assemblies.
- [ ] AWS read-only inventory identifies the deployed stack/configuration.
- [ ] End-to-end telemetry is recorded for the assigned device/vehicle only.

## Security

- [ ] User-to-vehicle authorization is implemented or beta access is explicitly
  limited to a trusted single-user environment.
- [ ] CORS and WebSocket token/log handling are reviewed.
- [ ] Every device has a unique certificate and revocation owner.
- [ ] OTA password is non-empty and uniquely handled.
- [ ] Open local AP/WebUI risk is accepted with clear tester instructions.
- [ ] Support collection and backups are redacted/private.

## Support and recovery

- [ ] First-line support owner and escalation path are assigned.
- [ ] Device replacement and certificate revocation responsibilities are assigned.
- [ ] Known-good USB recovery artifact/process exists for maintainers.
- [ ] Local OTA failure does not leave the tester without an escalation path.
- [ ] Factory reset limitations are explained.

## Documentation

- [ ] Beta guide matches the validated UI and version.
- [ ] Current screenshots are captured and redacted.
- [ ] Device label/variant matches the handoff record.
- [ ] Known limitations are acknowledged by the tester.
- [ ] Portal onboarding documentation matches implemented authorization.

## Decision

- [ ] Approved for bounded beta
- [ ] Approved with recorded exceptions
- [ ] Not approved

Record approver, date, release commit, device list and exceptions outside this
template as a release-specific validation artifact.
