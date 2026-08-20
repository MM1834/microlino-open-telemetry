# Email Delivery and Domain Operations

> **Status:** Deployed operational baseline
>
> **Audience:** Domain, mail and AWS administrator
>
> **Last verified:** 2026-08-20

## Delivery boundaries

MOT currently has two independent email paths. Changing one does not implicitly
change the other.

| Mail type | Service | Current sender | User control |
|---|---|---|---|
| Invitation, verification and password recovery | Cognito through SES | `MOT Portal <support@microlino-open-telemetry.ch>` | Administrator-controlled account lifecycle |
| SOC and journey notifications | SNS email subscriptions | AWS SNS sender; topic display name `Microlino Open Telemetry` | Explicit, default-off per-vehicle subscription |

The SES domain therefore improves account mail only. Product notifications do
not currently originate from `support@microlino-open-telemetry.ch`. Moving those
messages to the MOT sender requires a separate SNS-to-SES delivery change with
equivalent consent, unsubscribe, bounce/complaint and idempotency controls.

## Domain and mailbox requirements

- Keep a working, monitored `support@microlino-open-telemetry.ch` mailbox. Replies
  are delivered by the existing Hosttech MX records, not by SES.
- Do not store the mailbox password, a Cognito temporary password or another mail
  credential in the repository.
- Keep DNSSEC enabled and the existing DMARC policy
  `v=DMARC1; p=reject; pct=100` intact.
- Preserve the Hosttech MX records and unrelated A, AAAA and wildcard records.
- SES and Cognito are configured in `eu-north-1`. The verified identity and the
  Cognito user pool must remain region-compatible.
- Public self-registration remains disabled. Account creation and invitation are
  administrator controlled.

## Required SES Easy-DKIM CNAME records

Publish each record as CNAME. The host is the complete `_domainkey` name and the
canonical name is the matching `dkim.amazonses.com` target. A TTL of 10,800
seconds is acceptable.

| Host | Canonical name |
|---|---|
| `bzvgicucbnqoxjtt43bmgkvjvwiv26cu._domainkey.microlino-open-telemetry.ch` | `bzvgicucbnqoxjtt43bmgkvjvwiv26cu.dkim.amazonses.com` |
| `5yohyok46mmqjfrcipnkxjx732n3jak7._domainkey.microlino-open-telemetry.ch` | `5yohyok46mmqjfrcipnkxjx732n3jak7.dkim.amazonses.com` |
| `5zu6qdnvbo7shniiabza45lriny6j3wt._domainkey.microlino-open-telemetry.ch` | `5zu6qdnvbo7shniiabza45lriny6j3wt.dkim.amazonses.com` |

At DNS providers that automatically append the zone, enter only the host portion
through `._domainkey`; first verify the resulting fully qualified name in the
provider's record list. An accidental A record is not a substitute for the CNAME.

## Declared AWS configuration

`cloud/aws/foundation/template.yaml` owns the SES domain identity and the Cognito
email configuration. The operational values are:

- `CognitoEmailSendingAccount=DEVELOPER`;
- sender `MOT Portal <support@microlino-open-telemetry.ch>`;
- reply-to `support@microlino-open-telemetry.ch`;
- SES Easy DKIM, RSA 2048;
- shared SES sending, without dedicated IP addresses.

The safe rollout order is identity creation, CNAME publication, SES verification,
production-access approval, direct delivery test and only then the reviewed
Cognito switch to `DEVELOPER`. Do not select `DEVELOPER` while the domain identity
is unverified.

AWS granted transactional production access on 2026-08-20. The recorded quota is
50,000 messages per 24 hours and 14 messages per second. Quotas are ceilings, not
a target; monitor bounces and complaints and investigate unexpected volume.

## Verification

Check authoritative/public DNS rather than relying only on a local resolver:

```sh
dig +short CNAME bzvgicucbnqoxjtt43bmgkvjvwiv26cu._domainkey.microlino-open-telemetry.ch @1.1.1.1
dig +short CNAME 5yohyok46mmqjfrcipnkxjx732n3jak7._domainkey.microlino-open-telemetry.ch @1.1.1.1
dig +short CNAME 5zu6qdnvbo7shniiabza45lriny6j3wt._domainkey.microlino-open-telemetry.ch @1.1.1.1
```

In AWS, confirm that the SES identity is verified, DKIM is successful, production
access remains enabled and the Cognito user pool still uses the declared sender,
reply-to and identity ARN. Complete acceptance with an invitation and password
recovery to a controlled mailbox; verify delivery and the authentication results
in the received message headers.

## Authorization isolation

Email address is an identity attribute and delivery target, not vehicle
authorization. Access is derived from the authenticated Cognito `sub` and the
exact ACTIVE vehicle assignment.

The demo restrictions apply to the exact `demo-pioneer` assignment: notification
writes and claim consumption return 403, and the portal disables their controls.
The confirmed Gino identity is assigned to `ginopioneer`; it does not inherit the
demo boundary and continues to use normal notification and onboarding behavior.
