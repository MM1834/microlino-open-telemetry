# AWS-1 validation checklist

## Account and endpoint

- [ ] AWS account selected
- [ ] Region selected
- [ ] AWS CLI identity verified
- [ ] ATS data endpoint recorded

## Thing identity

- [ ] Thing name follows `mot-<board>-<suffix>`
- [ ] MQTT client ID equals Thing name
- [ ] Vehicle ID attribute is correct
- [ ] Certificate is attached exclusively
- [ ] Per-device policy attached

## Credentials

- [ ] Private key exists only in protected storage
- [ ] Credential folder ignored by Git
- [ ] Secure backup created
- [ ] Certificate ID recorded
- [ ] Revocation procedure understood

## Connectivity

- [ ] AWS MQTT test client subscribed
- [ ] Desktop TLS publish succeeds
- [ ] Publish appears on `mot/<vehicleId>/#`
- [ ] Wrong client ID is rejected
- [ ] Publish outside vehicle namespace is rejected

## Ready for AWS-2

- [ ] Endpoint
- [ ] Root CA
- [ ] Device certificate
- [ ] Device private key
- [ ] Thing/client ID
- [ ] Existing MOT topic prefix and vehicle ID
