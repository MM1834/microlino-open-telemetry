# AWS IoT credential handling

> 🔒 **Security:** Device private keys are production secrets.

## Files per manually provisioned beta device

```text
AmazonRootCA1.pem
device-certificate.pem.crt
device-private-key.pem.key
thing metadata
```

Only the Amazon root CA is public.

## Repository rules

Never commit:

```text
*.key
*-private.pem*
*-certificate.pem.crt
certs/device/*
secrets.h
aws_credentials.*
```

## Beta provisioning model

1. Create a Thing.
2. Create and activate a unique certificate.
3. Attach the certificate to the Thing.
4. Attach a least-privilege policy.
5. Install endpoint, CA, certificate and private key on exactly one device.
6. Record the certificate ID outside the public repository.
7. Test certificate deactivation/revocation.

## Storage on ESP32

Prototype options:

- private ignored header,
- private filesystem image,
- USB provisioning into NVS.

For an end-user release, prefer protected device storage rather than shared credentials compiled into public firmware binaries.

## Rotation and ownership transfer

The future lifecycle must define:

- certificate replacement,
- deactivation/revocation,
- factory-reset behavior,
- device reassignment,
- ownership transfer.

Factory Reset must not silently create a second unmanaged cloud identity.
