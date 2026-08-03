# AWS IoT roadmap

> **Status:** Planned roadmap; checkboxes indicate repository work, not deployment
>
> **Audience:** Maintainer, cloud and firmware developer

Checkboxes describe repository implementation, not independently verified deployed
AWS state. See [Current Status](../governance/CURRENT_STATUS.md) for the validation
boundary.

## AWS-1 — Foundation

- [x] Select an initial AWS account and Region
- [x] Support an AWS IoT data endpoint in device configuration
- [x] Define Thing naming and stable MQTT client identity
- [x] Provide manual Thing/bootstrap tooling
- [x] Use a unique certificate and private key per device
- [x] Document least-privilege IoT policy requirements
- [x] Document validation with the AWS MQTT test client
- [ ] Document cost and cleanup

## AWS-2 — ESP32-WROOM reference

- [x] Add MQTT/TLS configuration
- [x] Load Amazon root CA
- [x] Load device certificate and private key
- [x] Require valid UTC before connect
- [x] Keep existing MOT topics
- [x] Add Last Will and heartbeat
- [ ] Test reconnect and certificate rejection
- [ ] Measure flash, heap and connection latency

## AWS-3 — LilyGO WiFi

- [x] Port the same TLS identity model
- [x] Provide the WiFi AWS path
- [ ] Validate RAM and WebUI responsiveness
- [x] Keep the initial AWS path independent of LTE stabilization

## AWS-4 — Backend and users

- [x] Select Cognito User Pools
- [ ] Model user-to-vehicle ownership
- [x] Implement JWT-authenticated REST API routes
- [x] Add authenticated live telemetry delivery
- [ ] Add history storage
- [x] Migrate the AWS dashboard path away from device/broker credentials
- [ ] Enforce vehicle authorization in REST and WebSocket handlers
- [ ] Implement beta account invitation and device claiming
- [ ] Implement recovery, transfer, replacement and revocation flows

## AWS-5 — LilyGO LTE/TLS

- [ ] Stabilize MQTT over LTE first
- [ ] Establish valid modem/GPS/system UTC
- [ ] Validate TLS certificate chain
- [ ] Test reconnect/backoff
- [ ] Test watchdog and concurrent modem use
- [ ] Test weak-signal and power scenarios

## AWS-6 — Fleet and OTA

- [ ] Evaluate Fleet Provisioning
- [ ] Design manufacturing claim scope
- [ ] Implement certificate rotation
- [ ] Evaluate AWS IoT Jobs
- [ ] Sign firmware
- [ ] Add rollback/recovery reporting
