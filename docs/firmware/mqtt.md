# MQTT

The historical WROOM/LilyGO legacy-broker client uses plain MQTT over TCP and is
not an acceptable security baseline for a new deployment. MQTT-001 now has a
repository-build-qualified C6 path using MQTT over verified TLS only. It remains
default-off and physically unvalidated; do not treat it as released until the
Sprint's broker, ioBroker and N16 acceptance gates pass. The browser dashboard may
use MQTT over WebSocket, but that is a separate connection path and port.

## Secure C6 local MQTT boundary

The C6 implementation requires a configured broker hostname, TLS port, CA/trust
anchor, vehicle ID and one complete dedicated authentication mode:

- broker username plus password; or
- client certificate plus matching private key.

It does not provide plaintext, anonymous or certificate-verification bypass
modes. Credentials are stored separately from AWS IoT files under `/mqtt`, are
omitted from diagnostics and backup exports, and can be deleted only through the
authenticated same-origin configuration action. UTC must be plausible before a
TLS connection is attempted.

Local MQTT, MOT Cloud and ABRP are independent services. The local client only
publishes retained telemetry under the configured
`<prefix>/<vehicleId>/#` namespace, uses a retained `status/online` birth/Last
Will state and does not subscribe to command topics.

A private consumer may persist this state independently of MOT Cloud. The
[MQTT-to-MariaDB collector guide](../tools/mqtt-mariadb-collector.md) defines the
recommended subscribe-only identity, typed append-only storage, retained-message
handling and privacy boundary. Database access remains outside the firmware.

Location uses the same canonical suffixes but has a separate default-off consent
state. Only the authenticated local adapter UI can opt in. Opt-out stops new
location publishes and deletes retained location topics immediately or after the
next broker reconnect. It cannot erase copies already stored by a consumer.

## Topic pattern

```text
<mqttPrefix>/<vehicleId>/<category>/<field>
```

Typical default shape:

```text
mot/<vehicle>/<category>/<field>
```

## Telemetry topics

### Display

| Suffix | Type | Unit |
|---|---:|---|
| `display/soc` | number | % |
| `display/speed_kmh` | number | km/h |
| `display/odometer_km` | number | km |
| `display/estimated_range_km` | number | km |

### Charging

| Suffix | Type |
|---|---:|
| `charging/is_charging` | boolean |
| `charging/plugged` | boolean |
| `charging/power_display` | number |
| `charging/power_signed` | number |

### Location

| Suffix | Type | Unit |
|---|---:|---|
| `location/latitude` | number | degrees |
| `location/longitude` | number | degrees |
| `location/speed_kmph` | number | km/h |
| `location/altitude_m` | number | m, raw GNSS altitude |
| `location/satellites` | integer | — |
| `location/hdop` | number | — |
| `location/age_ms` | integer | ms |

Location, including `location/altitude_m`, is live-only. The C6 offline cache is
limited to SOC and Speed and replays exclusively through the acknowledged AWS
History backfill; enabling local MQTT does not redirect or drain that cache.

### System

The current dashboard work expects these system suffixes:

| Suffix | Purpose |
|---|---|
| `system/device_id` | Device identity |
| `system/device_name` | Human-readable name |
| `system/mqtt_client_id` | MQTT client identifier |
| `system/board` | Compile-time board identifier |
| `system/firmware_version` | Firmware release |
| `system/can1_profile` | Stable CAN1 decoder-profile key |
| `system/can2_profile` | Stable CAN2 decoder-profile key or `disabled` |
| `system/ip_address` | Current interface IP |
| `system/network_mode` | `WiFi` or `LTE` |
| `system/mqtt_transport` | MQTT transport |
| `system/wifi_rssi` | WiFi signal |
| `system/uptime_sec` | Uptime |

## Retained values

State values are generally retained so a dashboard can reconstruct the latest known state immediately after subscribing.

## Presence

A true device-online state should be based on device telemetry, heartbeat and ideally an MQTT Last Will, not solely on the browser's MQTT connection.

A practical topic is:

```text
<mqttPrefix>/<vehicleId>/status/online
```

The dashboard can additionally use a broker-side `last_seen` timestamp generated whenever a device topic is received.

## Client IDs

Firmware device IDs should remain stable. Browser dashboard IDs should be persisted in browser storage instead of creating a new random ID on every page load.

## PubSubClient states

Use the state text produced by the current firmware as the primary interpretation. Historic documents used different numeric mappings during development, so old sprint notes must not be treated as authoritative for current return-code numbers.
