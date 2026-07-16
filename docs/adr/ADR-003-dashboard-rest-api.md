# ADR-003 — Dashboard uses REST instead of browser MQTT

- **Status:** Accepted
- **Date:** 2026-07-16
- **Decision owners:** MOT maintainers
- **Related:** ADR-001, ADR-005, ADR-006, ADR-008

## Context

The first Dashboard connected directly to an MQTT broker over WebSocket. This
was effective for local development and made retained topics visible quickly.

However, direct browser MQTT created long-term product problems:

- the browser needed broker credentials
- every user became an MQTT client
- device transport details leaked into UI code
- vehicle authorization would need to be implemented as broker permissions
- multiple vehicles complicated subscriptions and state reset
- applications would need to understand retained topics and reconnect behavior
- mobile and third-party clients would repeat the same broker logic

The platform requires a clean boundary for future login, per-user vehicle
permissions and API evolution.

## Decision

The production Dashboard uses the Vehicle REST API and does not connect
directly to MQTT.

The application path is:

```text
Dashboard
  -> HTTPS
  -> API Gateway HTTP API
  -> Vehicle API Lambda
  -> DynamoDB current state
```

The Dashboard first requests the list of vehicles:

```text
GET /api/vehicles
```

It then polls the selected snapshot:

```text
GET /api/vehicles/{vehicleId}/snapshot
```

The current beta implementation polls every five seconds.

A provider boundary remains in the Dashboard so local legacy MQTT can still be
used as an explicit development mode.

## Data contract

A snapshot contains:

- `vehicleId`
- summary fields such as online and last seen
- `values` keyed by MQTT topic suffix
- optional metadata such as receive time and value type

Example:

```json
{
  "vehicleId": "pioneer",
  "online": true,
  "lastSeenUtc": 1784139635,
  "values": {
    "status/online": true,
    "system/last_seen_utc": 1784139635,
    "system/network_mode": "WiFi"
  }
}
```

## Alternatives considered

### Keep browser MQTT as the production architecture

Rejected because it couples user applications to broker credentials, MQTT
authorization and transport behavior.

### Connect the browser directly to AWS IoT Core over WebSockets

Rejected because user-to-vehicle authorization would become entangled with AWS
IoT permissions and each application would need to implement MQTT semantics.

### Use only a WebSocket application API

Deferred. Live updates are desirable, but a read-only snapshot API was the
smallest stable application boundary. WebSocket or Server-Sent Events can be
added later without changing the vehicle model.

### Use static exported JSON files

Rejected because they do not provide timely state, multi-vehicle listing or a
path to authenticated access.

## Consequences

### Positive

- no device certificate or broker password in the browser
- UI code is independent of MQTT reconnect and subscription logic
- multi-vehicle selection is straightforward
- current-state rendering is deterministic
- future Cognito authorization fits naturally
- mobile and other applications can use the same API
- backend changes can preserve the API contract

### Negative

- polling is not real-time
- API Gateway and Lambda add infrastructure and cost
- the current beta API must be protected before public use
- a current-state store is required
- five-second polling creates repeated HTTP requests
- live updates will require an additional application channel later

## UI state rule

Switching vehicles must clear all vehicle-specific values before applying the
new snapshot.

Values from the previously selected vehicle must never remain visible when the
new snapshot does not contain them.

Cloud/API connection state is separate from vehicle state and remains active
during vehicle switching.

## Security implication

The beta API is temporarily unauthenticated for validation only.

Before end-user deployment, the API must enforce:

- authenticated identity
- user-to-vehicle authorization
- restricted vehicle listing
- rate limits and abuse protection
- production CORS policy

## Validation evidence

The decision has been validated with:

- two vehicle IDs in DynamoDB
- vehicle list endpoint
- snapshot endpoint
- Dashboard selector
- independent online state
- correct IP/network update on switching
- clearing of missing SOC/ODO/charging values
- no active production MQTT connection from the browser

## Follow-up

- add Cognito JWT authorization
- add user-to-vehicle ownership mapping
- evaluate WebSocket or Server-Sent Events for live updates
- add API versioning before external integrations
