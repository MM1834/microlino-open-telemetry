# AWS IoT roadmap

## AWS-1 — Foundation

- [ ] Select AWS account and Region
- [ ] Record AWS IoT data endpoint
- [ ] Define Thing naming
- [ ] Create one test Thing
- [ ] Create unique certificate and private key
- [ ] Create least-privilege IoT policy
- [ ] Verify with AWS MQTT test client
- [ ] Document cost and cleanup

## AWS-2 — ESP32-WROOM reference

- [ ] Add MQTT/TLS configuration
- [ ] Validate Amazon root CA
- [ ] Load device certificate and private key
- [ ] Require valid UTC before connect
- [ ] Keep existing MOT topics
- [ ] Add Last Will and heartbeat
- [ ] Test reconnect and certificate rejection
- [ ] Measure flash, heap and connection latency

## AWS-3 — LilyGO WiFi

- [ ] Port the same TLS identity model
- [ ] Test over WiFi only
- [ ] Validate RAM and WebUI responsiveness
- [ ] Avoid modem/LTE changes

## AWS-4 — Backend and users

- [ ] Select user identity provider
- [ ] Model user-to-vehicle ownership
- [ ] Implement authenticated API
- [ ] Add live telemetry delivery
- [ ] Add history storage
- [ ] Migrate dashboard away from broker credentials

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
