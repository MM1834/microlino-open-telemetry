# Onboarding Claim Data Model

> **Status:** Planned design for ONB-001.B2 — not implemented
>
> **Audience:** Backend developer, security reviewer and beta administrator
>
> **Last verified:** 2026-08-03 against branch `codex/spr-0005-beta-onboarding-readiness`

## Purpose

Define the minimum records and atomic transaction for an authenticated portal user
to claim one controlled vehicle identity. The model follows ONB-001.A default-deny
authorization and does not expose AWS IoT credentials to the portal.

## Record set

```mermaid
erDiagram
    ONBOARDING_CLAIM ||--o| VEHICLE_OWNERSHIP : consumes_into
    VEHICLE_OWNERSHIP ||--|| USER_VEHICLE_ACCESS : authorizes
    ONBOARDING_CLAIM ||--o{ ONBOARDING_AUDIT_EVENT : records

    ONBOARDING_CLAIM {
      string claimId PK
      string vehicleId
      string proofHash
      string proofSalt
      string status
      number expiresAt
      number failedAttempts
      number maxAttempts
    }
    VEHICLE_OWNERSHIP {
      string vehicleId PK
      string ownerUserSub
      string status
      number version
    }
    USER_VEHICLE_ACCESS {
      string userSub PK
      string vehicleId SK
      string status
      string role
    }
    ONBOARDING_AUDIT_EVENT {
      string entityId PK
      string eventKey SK
      string eventType
      number occurredAt
    }
```

The logical JSON schemas are versioned under `cloud/aws/onboarding/schemas/`. The
deployed DynamoDB representation uses native string/number attributes.

### Onboarding claim

`claimId` is an opaque random identifier and is not a device, vehicle or Thing ID.
The user-facing claim value contains an independently random proof. Both values use
cryptographically secure randomness; the proof contains at least 128 bits.

The backend stores only:

```text
sha256("mot:onboarding-claim:v1\0" + claimId + "\0" + proofSalt + "\0" + proof)
```

encoded as `sha256:<base64url-digest>`. The plaintext proof must never enter logs,
URLs, analytics, screenshots, Git, DynamoDB or long-lived browser storage.

Claim states:

```text
ISSUED -> CONSUMED
ISSUED -> REVOKED
ISSUED -> EXPIRED
```

`expiresAt` is enforced in application conditions. DynamoDB TTL uses the same value
only for eventual cleanup; TTL deletion timing is never authorization evidence.
`failedAttempts >= maxAttempts` blocks further use even before expiry.

### Vehicle ownership

`VehicleOwnership` is the canonical uniqueness record missing from the current
user-partitioned access table. Exactly one ACTIVE owner is allowed per `vehicleId`
in B2. The existing `UserVehicleAccess` item remains the efficient authorization
projection used by REST and WebSocket paths.

Sharing, fleet roles and rental groups require a later explicit role model. They
must not weaken the OWNER uniqueness condition implicitly.

### Audit event

Audit records are append-only and contain identifiers/status codes, not email,
claim proof, token, certificate, free-form support text or telemetry. `entityId`
groups events by claim/vehicle and `eventKey` combines sortable UTC time with a
random event ID. Retention and export policy must be approved before deployment;
the schema permits an optional TTL but does not select a production duration.

## Atomic claim transaction

After authenticating the Cognito access token and hashing the supplied proof, one
DynamoDB `TransactWriteItems` operation must:

1. update the claim from `ISSUED` to `CONSUMED` only when hash, expiry and attempt
   conditions pass;
2. create `VehicleOwnership(vehicleId)` only when no ownership record exists;
3. create `UserVehicleAccess(userSub, vehicleId)` only when no assignment exists;
4. append a `CLAIM_CONSUMED` audit event only when its key does not exist.

Any condition failure aborts all four writes. The API returns one generic invalid or
unavailable-claim response so callers cannot distinguish unknown, expired, consumed,
revoked, conflicting or incorrectly entered claims.

Failed proof attempts use a separate conditional counter update. API throttling,
per-claim attempt limits and a short expiry bound online guessing. A successful
transaction cannot be replayed because the claim is no longer `ISSUED`.

## Administrator issue flow

An administrator can issue a claim only for an inventory-approved `vehicleId` that
has no ACTIVE ownership. Issuance records `issuedBySub` or a controlled operator
reference, but never an email address. The plaintext proof is displayed exactly
once for protected handoff or QR generation and is immediately discarded by the
backend.

The current B1 CLI assignment path remains available for the small controlled beta.
It does not pretend to be atomic ownership claiming and must not run concurrently
for the same vehicle.

## Adapter replacement and reset boundary

B2 claims ownership of a portal vehicle identity; it does not provision or move an
AWS IoT certificate. B3 retains the `vehicleId` by default during replacement while
creating a new `deviceId`, Thing and certificate; intentional parallel beta units
may instead keep separate portal identities. Defect, loss and factory reset never
authorize a new owner or cloud identity by themselves.

## Cost boundary

B2 adds three small DynamoDB on-demand tables and a few Lambda/API/DynamoDB
operations per invitation or claim. At beta scale this is negligible compared with
continuous telemetry ingestion. CloudWatch measured roughly 2.78 million
`state-ingest` Lambda invocations between 2026-07-01 and 2026-08-04, versus roughly
10 thousand Vehicle API calls and fewer than one thousand live-handler calls.

Before fleet growth, measure MQTT publishes per device and evaluate batching or a
coarser state envelope. Keep B2 logs minimal with bounded retention, avoid custom
high-cardinality metrics and use pay-per-request resources with no idle servers.

## Deployment boundary

The foundation template is within 37 bytes of the 51,200-byte inline API limit.
B2 uses a separate `cloud/aws/onboarding/` stack and packaged Lambda code. It may
consume foundation outputs/parameters but must not add another inline Lambda to the
foundation template.

No stack, table, secret, claim or API route is created by this design step.

## Related documents

- [ONB-001.B work package](../project/sprints/ONB-001-B.md)
- [Authorization foundation](onboarding-authorization.md)
- [AWS IoT architecture](aws-iot.md)
- [Cloud risks](../security/cloud-risk-register.md)
