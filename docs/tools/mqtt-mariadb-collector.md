# MQTT to MariaDB Collector

> **Status:** Private, optional integration guide
>
> **Audience:** Operator of a private MOT broker and MariaDB instance
>
> **Last reviewed:** 2026-09-15 against the MQTT-001 C6 topic contract

## Purpose and boundary

This guide describes a small, private collector that subscribes to the secure
local MOT MQTT output and stores scalar telemetry in MariaDB for later Grafana
visualization.

```text
MOT adapter --MQTT/TLS--> private broker --MQTT/TLS--> collector --> MariaDB
                                                                  --> Grafana
```

The collector is not part of the firmware, MOT Cloud or the public MOT portal.
It must not use AWS IoT certificates. Local MQTT and MOT Cloud remain independent
adapter settings; disabling MOT Cloud is a separate operator decision and is the
only way this path can reduce AWS ingest and storage use.

The collector stores the MQTT broker receive time because ordinary MOT telemetry
payloads contain one scalar value rather than an event timestamp. It therefore
cannot reconstruct samples missed while the collector or broker was unavailable.
The retained broker value is current state, not historical evidence.

The authoritative topic and unit list is the
[MQTT topic contract](../api/mqtt-topics.md). Do not infer a new schema from old
WROOM/LilyGO MQTT payloads.

If ioBroker already receives the MQTT states and persists them through
`ioBroker.sql`, do not add this collector merely for Grafana. Use the existing
SQL-history tables and the
[Grafana with ioBroker MySQL guide](grafana-iobroker-mysql.md). The standalone
collector below is intended only when ioBroker history is not the selected
storage boundary.

## Prerequisites

- MQTT-001 local MQTT configured with TLS and a dedicated adapter identity;
- a private broker reachable by both adapter and collector;
- a separate read-only MQTT identity for the collector;
- MariaDB with TLS when it is not on the collector host;
- synchronized UTC on the broker/collector host;
- Python 3 with `paho-mqtt` and `mariadb` available in an isolated virtual
  environment;
- an explicit decision whether location may be persisted. This guide defaults
  to no location storage.

Do not expose MQTT, MariaDB or Grafana directly to the public Internet. Use a
private network or VPN and restrict each firewall rule to the required hosts.

## 1. Create the broker consumer identity

The adapter has publish-only access to its own namespace. Create a different
broker account for the collector and grant it subscribe-only access to the
approved vehicles. A Mosquitto ACL for two vehicles can use:

```text
user mot-mariadb-collector
topic read mot/pioneer/#
topic read mot/private-test/#
```

Do not grant the collector write access. Do not subscribe to `mot/#` unless every
vehicle on that broker is intentionally in scope. Configure the collector with
the broker CA and keep hostname verification enabled.

First inspect the contract without printing credentials:

```bash
mosquitto_sub \
  --host mqtt.internal.example \
  --port 8883 \
  --cafile /etc/mot-collector/broker-ca.pem \
  --username mot-mariadb-collector \
  --pw 'REPLACE_INTERACTIVELY' \
  --topic 'mot/pioneer/#' \
  --verbose
```

Prefer an interactive secret mechanism or a protected password file in normal
operation because a command-line password can be visible to other local users.
Expected payloads are numbers, `true`/`false`, and bounded system strings. They
are not one combined JSON document.

## 2. Create the MariaDB database and restricted account

Run the administrative part through a local MariaDB administrator. Replace the
example host with the exact collector host; do not use `%` for convenience.

```sql
CREATE DATABASE mot_telemetry
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

CREATE USER 'mot_collector'@'collector.internal.example'
  IDENTIFIED BY 'REPLACE_WITH_A_RANDOM_SECRET';

GRANT SELECT, INSERT, UPDATE, DELETE
  ON mot_telemetry.*
  TO 'mot_collector'@'collector.internal.example';
```

Grafana needs a separate account:

```sql
CREATE USER 'mot_grafana'@'grafana.internal.example'
  IDENTIFIED BY 'REPLACE_WITH_A_DIFFERENT_RANDOM_SECRET';

GRANT SELECT
  ON mot_telemetry.*
  TO 'mot_grafana'@'grafana.internal.example';
```

`DELETE` is included for the collector so retained location tombstones and the
configured retention job can remove data. It must not receive schema or user
administration privileges after initial setup.

## 3. Create the storage schema

Store history and latest state separately. This makes Grafana time-series
queries efficient while preventing retained reconnect snapshots from becoming
false historical samples.

```sql
USE mot_telemetry;

CREATE TABLE telemetry_samples (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  vehicle_id VARCHAR(64) NOT NULL,
  signal_name VARCHAR(96) NOT NULL,
  observed_at DATETIME(6) NOT NULL,
  numeric_value DOUBLE NULL,
  boolean_value BOOLEAN NULL,
  string_value VARCHAR(512) NULL,
  PRIMARY KEY (id),
  INDEX ix_samples_vehicle_signal_time
    (vehicle_id, signal_name, observed_at),
  INDEX ix_samples_time (observed_at)
) ENGINE=InnoDB;

CREATE TABLE telemetry_latest (
  vehicle_id VARCHAR(64) NOT NULL,
  signal_name VARCHAR(96) NOT NULL,
  observed_at DATETIME(6) NOT NULL,
  numeric_value DOUBLE NULL,
  boolean_value BOOLEAN NULL,
  string_value VARCHAR(512) NULL,
  PRIMARY KEY (vehicle_id, signal_name),
  INDEX ix_latest_time (observed_at)
) ENGINE=InnoDB;
```

Use UTC for every database session:

```sql
SET time_zone = '+00:00';
```

`signal_name` is the suffix after `mot/<vehicleId>/`, for example
`display/soc` or `bms/vehicle_power_w`. Do not duplicate the complete MQTT topic
in every row.

## 4. Collector behavior

The collector should implement the following rules before it is trusted with
vehicle data:

1. Connect to MQTT only with TLS certificate and hostname verification.
2. Authenticate with its dedicated subscribe-only identity.
3. Subscribe only to explicitly configured `mot/<vehicleId>/#` filters.
4. Accept exactly four topic segments: prefix, vehicle, category and field.
5. Reject a vehicle ID not present in the local allowlist.
6. Parse each known suffix according to the type table below.
7. Attach the collector's UTC receive time in one database transaction.
8. Always upsert `telemetry_latest`.
9. Insert into `telemetry_samples` only when the MQTT message is not a retained
   snapshot delivered during subscription.
10. Treat an empty retained location payload as a tombstone: delete the matching
    latest row and do not add a history sample.
11. Reconnect with bounded exponential backoff and health logging, without
    logging payload secrets or database credentials.

Recommended type groups:

| Type | Signals |
|---|---|
| Boolean | `charging/is_charging`, `charging/plugged`, `bms/is_regenerating`, `bms/is_discharging`, `status/online` |
| Numeric | all `display/*`; charging power; numeric `bms/*`; numeric `location/*`; `system/wifi_rssi`, `system/uptime_sec`, `system/last_seen_utc` |
| String | bounded `system/*` identity, version, profile, address and transport values |

Reject non-finite numbers, values outside the documented physical/unit bounds,
unknown topic shapes and payloads larger than the expected scalar limit. Unknown
but well-formed signals should be logged as unsupported instead of being silently
stored with a guessed type.

The database transaction for a normal message is conceptually:

```sql
START TRANSACTION;

INSERT INTO telemetry_latest
  (vehicle_id, signal_name, observed_at,
   numeric_value, boolean_value, string_value)
VALUES (?, ?, UTC_TIMESTAMP(6), ?, ?, ?)
ON DUPLICATE KEY UPDATE
  observed_at = VALUES(observed_at),
  numeric_value = VALUES(numeric_value),
  boolean_value = VALUES(boolean_value),
  string_value = VALUES(string_value);

-- Execute only for a live, non-retained MQTT delivery.
INSERT INTO telemetry_samples
  (vehicle_id, signal_name, observed_at,
   numeric_value, boolean_value, string_value)
VALUES (?, ?, UTC_TIMESTAMP(6), ?, ?, ?);

COMMIT;
```

Generate one receive timestamp in the collector and bind the same timestamp to
both statements. `UTC_TIMESTAMP(6)` above only illustrates the required value.

## 5. Configuration and secret handling

Keep configuration separate from code. A root-owned environment file can contain
names and paths, but its actual secrets must never enter this repository:

```text
MQTT_HOST=mqtt.internal.example
MQTT_PORT=8883
MQTT_CA_FILE=/etc/mot-collector/broker-ca.pem
MQTT_USERNAME=mot-mariadb-collector
MQTT_PASSWORD_FILE=/etc/mot-collector/mqtt-password
MQTT_TOPICS=mot/pioneer/#,mot/private-test/#
MQTT_VEHICLES=pioneer,private-test
MARIADB_HOST=mariadb.internal.example
MARIADB_PORT=3306
MARIADB_DATABASE=mot_telemetry
MARIADB_USERNAME=mot_collector
MARIADB_PASSWORD_FILE=/etc/mot-collector/mariadb-password
STORE_LOCATION=false
```

Set the environment file, password files and CA file readable only by the
collector service account. Give the service a stable MQTT client ID so a second
accidental instance cannot create ambiguous concurrent ingestion.

`STORE_LOCATION=false` must reject every `location/*` message. Enabling it is an
explicit private deployment decision and does not replace the adapter owner's
separate location opt-in.

## 6. Service operation

Run exactly one active collector for a given vehicle/database target. Configure:

- automatic restart after failure with bounded delay;
- a process memory limit;
- read-only code and configuration mounts where possible;
- a writable runtime directory only when the client library needs one;
- local log rotation without payload dumps;
- database connection retry independent of MQTT reconnect;
- graceful shutdown that commits or rolls back the current transaction.

Do not acknowledge operational health merely because the process is running.
Health requires a recent broker connection, successful database write and a
plausibly recent `system/last_seen_utc` or live message for each expected vehicle.

## 7. Retention and privacy

Define retention before enabling continuous collection. For a private pilot, a
starting point is 90 days of scalar telemetry and a shorter or disabled location
history. Run deletion in bounded batches during a quiet period, for example:

```sql
DELETE FROM telemetry_samples
WHERE observed_at < UTC_TIMESTAMP() - INTERVAL 90 DAY
ORDER BY observed_at
LIMIT 10000;
```

Repeat until no eligible rows remain rather than issuing one unbounded delete.
Back up MariaDB only if the backup has the same access, retention and deletion
rules as the live database.

An MQTT retained tombstone removes broker state, not copies already stored in
MariaDB or backups. If location storage is enabled, define whether location
opt-out deletes all stored `location/*` rows for that vehicle. The privacy-safe
default is to delete them and then apply the same deletion to managed backups
according to their lifecycle.

## 8. Acceptance checks

Complete these checks with a disposable test vehicle identity first:

- allowed topics arrive; an unapproved vehicle namespace does not;
- numeric, boolean and string values use the expected database column;
- collector restart updates latest state but does not add retained snapshots to
  history;
- a new live update creates exactly one history row;
- invalid payloads and unknown topics are rejected and counted;
- broker and MariaDB outages recover without a busy reconnect loop;
- duplicate collector instances are detected or prevented;
- `STORE_LOCATION=false` leaves no `location/*` rows;
- when location is enabled, retained tombstones clear latest state and the
  selected historical deletion policy is demonstrated;
- the Grafana account can select but cannot insert, update or delete;
- timestamps render correctly across UTC and `Europe/Zurich`, including daylight
  saving transitions.

Useful verification queries:

```sql
SELECT vehicle_id, signal_name, observed_at,
       numeric_value, boolean_value, string_value
FROM telemetry_latest
ORDER BY vehicle_id, signal_name;

SELECT vehicle_id, signal_name, COUNT(*) AS samples,
       MIN(observed_at) AS first_sample,
       MAX(observed_at) AS last_sample
FROM telemetry_samples
GROUP BY vehicle_id, signal_name
ORDER BY vehicle_id, signal_name;

SELECT COUNT(*) AS forbidden_location_rows
FROM telemetry_samples
WHERE signal_name LIKE 'location/%';
```

## 9. Grafana connection

Add MariaDB through Grafana's built-in MySQL data source using only the
`mot_grafana` account. Set the database session timezone to UTC and let Grafana
render the viewer's timezone.

A first SOC panel can use:

```sql
SELECT
  observed_at AS time,
  numeric_value AS value
FROM telemetry_samples
WHERE vehicle_id = ${vehicle_id:sqlstring}
  AND signal_name = 'display/soc'
  AND $__timeFilter(observed_at)
ORDER BY observed_at;
```

Create the `$vehicle_id` variable from an explicit allowlist or a restricted
query. Raw telemetry is enough for live and time-series panels. TeslaMate-style
drive, charging and efficiency dashboards additionally require derived journey
and charging-session tables or views; build those as a separate work package so
the collector remains a small, auditable ingestion boundary.

## Limitations

- MQTT-001 publishes current scalar states, not complete journey objects.
- Retained MQTT messages provide only the newest value.
- The current local MQTT path does not provide guaranteed store-and-forward for
  every MariaDB sample.
- Broker receive time is not the same as original CAN observation time.
- Energy integration and journey boundaries require gap handling and must not
  silently interpolate across collector outages.
- This private integration does not inherit MOT Cloud user authorization,
  notification processing or portal access controls.
