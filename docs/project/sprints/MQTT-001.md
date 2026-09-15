# MQTT-001 — Secure Local MQTT Integration

> **Status:** In progress — C6 repository slice build-qualified; first ioBroker TLS path confirmed
>
> **Prepared:** 2026-09-08
>
> **Started:** 2026-09-08
>
> **Priority:** Medium

## Objective

Restore an optional local MQTT telemetry output for the shared MOT firmware with
mandatory transport encryption, broker authentication and a bounded
publish-only authorization model. Qualify the ESP32-C6 N16 first. Preserve a
board-neutral boundary for a later 16 MB LilyGO T-SIM7670G-S3/SIM7670 extension,
but defer its implementation and mobile-network acceptance until the hardware
decision is final.

The first real consumer and test path is the ioBroker MQTT client/adapter. Home
Assistant must be able to consume the identical state and availability topics;
its optional discovery/configuration topics are a later metadata layer and must
not require a second firmware telemetry schema. MQTT Discovery follows only after
stable identities, units, availability and retained lifecycle have contract tests.

## Product and architecture decision

- Local MQTT is a separately selectable telemetry transport alongside MOT Cloud
  and ABRP; none of the three services implicitly enables or disables another.
- The new implementation starts on C6 but must keep the transport, topic and
  configuration components board-neutral for later LilyGO reuse. Existing
  WROOM/LilyGO plaintext MQTT code is historical implementation evidence only and
  is not an acceptable transport or security baseline.
- C6 N16 is the primary implementation and hardware-acceptance target.
- The intended later LilyGO target is the 16 MB T-SIM7670G-S3. It must ultimately
  support secure MQTT over WiFi and, where the configured broker is reachable,
  the mobile network. Both transports must apply identical TLS, hostname and
  authentication checks, with no plaintext or certificate-bypass fallback. This
  target is parked until the hardware decision is final and is not a completion
  gate for the initial C6 sprint.
- XIAO C6 remains a compile and size-regression target only unless separately
  promoted to physical acceptance. WROOM and 4 MB LilyGO receive no new product
  qualification from this sprint.
- MQTT is telemetry output only. The device does not subscribe to generic command
  topics and never exposes its local HTTP API or AWS IoT credentials to a broker.

## Functional scope

- add `localMqttEnabled` independently of MOT Cloud and ABRP;
- configure broker hostname, TLS port, topic prefix and stable client identity;
- support broker authentication using either:
  - a dedicated username and password; or
  - a dedicated client certificate and private key;
- require a user-supplied CA/trust anchor and verify the broker certificate
  against the configured hostname in both authentication modes;
- publish under the exact configured `mot/<vehicleId>/#` namespace by default;
- publish canonical numeric, boolean and string payload types rather than
  preserving the legacy `0`/`1` boolean encoding;
- publish retained current state, a retained online birth state, a retained Last
  Will offline state and freshness metadata;
- use bounded reconnect/backoff with jitter and diagnostics that distinguish DNS,
  TCP, TLS trust, hostname, credential and authorization failures;
- expose authenticated local configuration, connection test, status and explicit
  credential delete/replace actions;
- expose a separate, default-off location-sharing control that can be enabled
  only by the owner through the authenticated local adapter UI;
- keep CAN processing, local WebUI, MOT Cloud and ABRP responsive while local MQTT
  is disconnected, timing out or backing off;
- preserve configuration across OTA and provide a documented migration from old
  plaintext MQTT settings without automatically activating them.

## Mandatory security boundary

### Transport security

- MQTT over TLS is mandatory; default to port 8883 while allowing another
  explicitly configured TLS port.
- Require TLS 1.2 or later as supported by the target and validate the complete
  server chain plus broker hostname or IP subject alternative name.
- Do not provide `insecure`, trust-all, expired-certificate, hostname-bypass or
  plaintext compatibility modes.
- Connection tests must fail closed if device UTC is unavailable or implausible
  for certificate validation. Recovery guidance may identify the clock problem
  without bypassing certificate validity.
- CA replacement and client-certificate replacement are explicit authenticated
  operations. A failed replacement must retain or recover the last complete valid
  credential set atomically.

### Authentication and authorization

- Anonymous broker access is prohibited.
- Every adapter uses credentials dedicated to that physical device and broker.
- Broker ACL guidance must restrict the client to connect with its exact client
  ID and publish/retain only below its exact vehicle namespace.
- Reusing AWS IoT device certificates, ABRP secrets, another vehicle's broker
  credentials or a shared fleet-wide password is prohibited.
- No subscribe or receive permission is required for the first release. Remote
  command/control is explicitly outside this sprint.

### Secret lifecycle

- Store broker password, client private key and client certificate separately
  from AWS IoT credentials and other service secrets.
- Never return secrets through status, diagnostics, logs, HTML, normal JSON
  responses or non-secret configuration backups.
- Blank form fields retain an existing secret. Replacement requires an explicit
  authenticated action; deletion requires a separate confirmation and disables
  local MQTT until a complete credential set exists.
- Validate uploaded CA/certificate/key size, format and pairing before commit.
- Document initial provisioning, backup exclusion, CA/server-certificate
  rotation, client-certificate expiry, broker replacement and factory-reset
  recovery.

## Topic and state contract

The existing [MQTT topic contract](../../api/mqtt-topics.md) remains the source of
canonical suffix names. The first acceptance set is:

- `display/soc`, `display/estimated_range_km`, `display/speed_kmh` and
  `display/odometer_km`;
- `charging/is_charging`, `charging/plugged`, `charging/power_signed` and, when
  available, `charging/power_display`;
- canonical BMS power/voltage/current values when valid for the selected decoder;
- `status/online`, `system/last_seen_utc`, firmware, board and decoder identities.

Location topics are part of the schema but remain disabled by default. They may
be published only after a separate affirmative opt-in by the owner through the
authenticated local adapter UI. Enabling local MQTT alone, restoring a backup or
enabling location on another transport must not imply this consent. The UI must
identify the exact location topics, retained-state effect and privacy consequence
before saving the opt-in.

Disabling the opt-in stops all new location publications and publishes retained
tombstones for every location topic. If the broker is unavailable, the adapter
must remember the pending cleanup and send the tombstones before any later
location value after reconnect. Documentation must state that MOT cannot erase
copies already consumed or independently stored by ioBroker, Home Assistant or
another broker client.

The implementation must define and contract-test:

- stable topic prefix, vehicle ID and client ID validation;
- JSON-style boolean payloads, numeric precision, units and unavailable values;
- retained-state replacement and cleanup when vehicle identity or topic prefix
  changes;
- independent location-consent state, retained location cleanup and prevention of
  consent through backup restore or configuration migration;
- birth, Last Will, graceful shutdown and stale-data semantics;
- immediate plug/charge/online transitions plus a bounded periodic cadence for
  other values;
- a versioned migration rule for consumers of the old `0`/`1` boolean payloads.

## Service isolation and resource limits

- Local MQTT, MOT Cloud and ABRP have independent enable flags, credentials,
  runtime state, reconnect schedules and diagnostics.
- Failure of one network service must not block CAN reception/decoding, GPS, the
  authenticated local WebUI or another transport.
- Network work is absent while local MQTT is disabled.
- TLS buffers, credential storage, task stack, queue depth and reconnect rate are
  bounded and measured on C6 N16 before LilyGO activation.
- Publishing uses the canonical telemetry snapshot and must not add a second CAN
  decoding or vehicle-specific signal path.
- Do not publish the same local topic twice through parallel legacy and new
  clients. Old plaintext configuration is imported only into a disabled review
  state and must be completed with trust and authentication material before use.

## Work packages

### A — Contract and threat-model gate

- freeze configuration schema, credential lifecycle and topic/payload contract;
- document trust, broker ACL, privacy and failure boundaries;
- add migration fixtures for old plaintext settings and boolean payloads;
- define the exact C6 memory/flash baseline before implementation and record the
  later LilyGO baseline only when its parked extension is resumed.

### B — Shared secure MQTT client

- implement board-neutral TLS/authentication, publish queue, retained birth/Will,
  reconnect/backoff, typed payload and diagnostics components;
- add injectable platform adapters for secure storage, clock, network client and
  board runtime;
- add focused unit/contract tests including certificate, hostname, credential,
  ACL and outage failures.

### C — C6 N16 integration and acceptance

- integrate configuration and authenticated local UI into the shared C6 runtime;
- prove independent MOT Cloud/ABRP/local-MQTT combinations;
- build all maintained C6 environments and record binary, heap, largest-block and
  reconnect measurements;
- complete broker, outage, reboot, OTA-preservation and 12-hour soak tests on a
  physical N16 before another board is promoted.

### D — ioBroker acceptance and Home Assistant compatibility

- provide least-privilege Mosquitto TLS/user and TLS/client-certificate examples;
- verify the core, availability, freshness and opted-in location topics and types
  through the ioBroker MQTT adapter as the primary consumer acceptance;
- document Home Assistant mappings with units, device classes, state classes,
  availability and freshness presentation against the same state-topic schema;
- prove that neither consumer needs AWS certificates or access to the device-local
  HTTP API;
- retain Home Assistant MQTT Discovery/configuration topics as a separate reviewed
  metadata increment after the state schema is stable. Discovery must not fork or
  rename the underlying telemetry topics used by ioBroker.

### E — Parked LilyGO 16 MB/SIM7670 extension

Do not begin this work package until the hardware decision explicitly resumes it.
At that point:

- reuse the shared client and configuration without a LilyGO-specific protocol;
- qualify WiFi operation on the selected 16 MB LilyGO hardware;
- support the mobile network as a required target capability and separately test
  LTE DNS, trusted TLS, authentication, reconnect, modem recovery and return to
  preferred WiFi against a reachable broker;
- demonstrate that LTE failure cannot trigger plaintext fallback, block CAN or
  expose credentials;
- record flash, heap and long-running TLS/modem evidence before claiming LilyGO
  acceptance.

## Repository acceptance gates

- [ ] Threat model, configuration schema and versioned topic contract are reviewed.
- [ ] No plaintext, anonymous or certificate-bypass code path is reachable.
- [ ] Wrong CA, hostname, expired certificate, wrong password, wrong client key
  and denied publish namespace all fail closed with redacted diagnostics.
- [ ] Secrets are absent from normal exports, responses, logs and repository
  fixtures; blank submissions retain them and confirmed deletion removes them.
- [ ] Local MQTT remains disabled after migration until TLS and authentication
  prerequisites are complete.
- [x] Maintained C6 targets compile; existing AWS/ABRP/security tests remain
  green. LilyGO becomes a required build gate only when work package E resumes.
- [ ] Topic values, types, retain flags, Will/birth and freshness semantics have
  automated contract tests.
- [ ] Disabling local MQTT produces no broker DNS, TCP or TLS traffic.
- [ ] Location is absent by default, cannot be enabled remotely or by migration,
  and requires a separate authenticated local owner action.
- [ ] Revoking location opt-in stops publication and clears retained broker
  location state immediately or through a durable cleanup on reconnect.
- [ ] Reconnect/backoff is bounded and CAN processing plus other services remain
  operational throughout broker and network outages.

## Physical and consumer acceptance gates

- [ ] C6 N16 connects to Mosquitto using username/password plus verified server
  TLS and publishes only its allowed namespace.
- [ ] C6 N16 connects in mutual-TLS mode with a dedicated client identity.
- [ ] Broker-side ACL denial of another vehicle namespace is demonstrated.
- [ ] Reboot, broker restart, CA/certificate rotation and configuration-preserving
  OTA recover without secret disclosure or retained-state ambiguity.
- [ ] ioBroker receives the same entities and payload types through its MQTT
  adapter without a translation-specific firmware mode, including the explicit
  location opt-in and opt-out cleanup.
- [ ] Home Assistant mappings are contract-checked against the same telemetry
  schema; a productive Home Assistant instance is not required for initial sprint
  completion.
- [ ] A 12-hour C6 N16 soak with MOT Cloud and local MQTT enabled shows no
  cumulative heap loss, task starvation or CAN regression.
- [ ] After work package E is resumed, LilyGO 16 MB WiFi and mobile-network
  acceptance pass the identical TLS/authentication, consent and outage gates
  before LilyGO support is claimed. This is not an initial C6 completion gate.

## Rollout and rollback

1. Ship the setting disabled by default.
2. Enable one dedicated test broker/client namespace on C6 N16.
3. Complete negative security tests before publishing real vehicle telemetry.
4. Validate ioBroker, contract-check the Home Assistant mapping and then complete
   the bounded N16 soak.
5. Keep the 16 MB LilyGO/mobile-network extension parked until the definitive
   hardware decision; resume it as work package E without changing the shared
   security or telemetry contract.
6. Roll back by disabling local MQTT while retaining unrelated WiFi, MOT Cloud,
   ABRP, CAN and AWS credentials. Credential deletion remains a separate action.

## Explicit non-goals

- cloud-side Home Assistant or ioBroker integration through the MOT REST/WebSocket
  APIs;
- unencrypted MQTT, anonymous access or an `insecure` TLS switch;
- importing or exposing AWS IoT private keys;
- generic MQTT command/control, remote configuration or actuator output;
- public Internet exposure of the local firmware HTTP API;
- automatic MQTT Discovery in the first acceptance increment;
- location export without a separate authenticated local owner opt-in;
- physical qualification of XIAO, WROOM or the older 4 MB LilyGO target.

## Documentation outputs

- update firmware MQTT and configuration owner pages with the final secure
  contract and migration behavior;
- add broker provisioning and certificate-rotation operator guidance;
- add ioBroker instructions and Home Assistant schema/mapping guidance;
- record exact build, security-negative, physical and soak evidence in this sprint
  before changing its status to complete.

## Implementation evidence — 2026-09-08

The first C6 slice is implemented in the repository:

- a default-off local MQTT runtime uses `WiFiClientSecure` plus `PubSubClient`
  with a mandatory user-supplied CA and no `setInsecure` or plaintext client;
- dedicated username/password and mTLS modes are present; uploaded certificates
  are parsed and an mTLS certificate/private-key pair is cryptographically checked
  before it is accepted;
- broker credentials live below a separate `/mqtt` LittleFS namespace, remain
  absent from normal configuration exports and diagnostics, and have an
  authenticated same-origin confirmed delete operation;
- hostname, port, vehicle namespace, topic prefix and authentication selection are
  stored independently from MOT Cloud and ABRP;
- the cooperative C6 loop runs local MQTT independently with bounded TLS/socket
  timeouts and exponential reconnect plus jitter;
- current display, charging, BMS, system and freshness values use canonical topic
  suffixes and typed boolean/numeric payloads with retained birth/Last Will state;
- location is default-off and controlled by a separate authenticated same-origin
  local action. Revocation persists a cleanup flag and publishes retained
  tombstones before any later location values after reconnect;
- backup restore explicitly forces local MQTT and location consent off.

The complete focused C6 suite passes 60 tests. The `nanoesp32c6-n16` build passes
at 1,404,108 used application bytes (26.8% of its 5,242,880-byte slot) and 59,096
bytes static RAM (18.0%). The compatibility-only `xiao-esp32c6` build passes at
1,389,896 bytes (81.6% of its 1,703,936-byte slot) and 58,356 bytes static RAM
(17.8%), remaining below the established 85% XIAO gate. This is repository and
build evidence only. Broker-negative tests, ioBroker acceptance, physical N16
runtime, resource/soak measurements, immediate transition publication, atomic
multi-file credential rotation and the parked LilyGO work remain open.

## Field evidence — 2026-09-15

The maintainer configured an ioBroker MQTT instance as the broker, enabled its
TLS listener and dedicated username/password authentication, and provisioned the
firmware with the matching locally issued CA trust anchor. The secure local MQTT
client connected successfully and vehicle telemetry arrived as ioBroker data
points. This provides the first real broker-connectivity and primary-consumer
receipt evidence for the username/password mode.

The observation does not yet prove the complete consumer gate: exact topic and
payload coverage, retain flags, Birth/Last Will, freshness, location opt-in and
retained cleanup still require recorded inspection. Wrong trust, hostname,
password and namespace denial, mTLS, broker/reboot/certificate recovery, outage
isolation and the 12-hour N16 soak likewise remain open.

The maintainer then completed the proposed follow-up sequence on 2026-09-15:
adapter restart, ioBroker MQTT-adapter stop/start, inspection of online/retained/
freshness behavior, and negative attempts with invalid credentials or trust
material all behaved as expected. Automatic broker reconnect succeeded and
telemetry resumed. This supplies real restart, broker interruption and basic
fail-closed evidence, but does not close the broader combined gates for exact
namespace ACL denial, certificate rotation, configuration-preserving OTA,
location retained-topic cleanup, mTLS, extended outage isolation or the 12-hour
soak.
