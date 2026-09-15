# Grafana with ioBroker MySQL History

> **Status:** Private pilot guide
>
> **Audience:** Operator of ioBroker, MySQL/MariaDB and Grafana
>
> **Last reviewed:** 2026-09-15

## Purpose

This guide imports the first MOT Grafana panel directly against an existing
ioBroker SQL-history database. It does not require a second collector or a
separate MariaDB server.

The supplied dashboard reads the numeric ioBroker state:

```text
mqtt.2.mot.pioneer.display.soc
```

from the standard `datapoints` and `ts_number` tables. It displays the last 24
hours of SOC as a step line.

Dashboard file:
[MOT Pioneer SOC](../../tools/grafana/mot-pioneer-soc.json)

TeslaMate-inspired overview:
[MOT vehicle overview](../../tools/grafana/mot-vehicle-overview.json)

Trips and energy dashboard:
[MOT trips and energy](../../tools/grafana/mot-trips-energy.json)

## 1. Confirm that ioBroker stores the state

Seeing an object below `mqtt.2.mot.pioneer` in ioBroker does not by itself mean
that SQL history is enabled. In the ioBroker object view, enable history for
`mqtt.2.mot.pioneer.display.soc` using the SQL/MySQL history adapter and wait for
at least two real updates.

Use the SQL adapter's configured database, host, user and database type to create
the Grafana data source. Do not change ioBroker's tables manually.

With a database client, verify the data-point name:

```sql
SELECT id, name, type
FROM datapoints
WHERE name = 'mqtt.2.mot.pioneer.display.soc';
```

For a numeric state, verify recent samples:

```sql
SELECT
  t.ts,
  FROM_UNIXTIME(t.ts / 1000.0) AS database_local_time,
  t.val
FROM ts_number AS t
INNER JOIN datapoints AS d ON d.id = t.id
WHERE d.name = 'mqtt.2.mot.pioneer.display.soc'
ORDER BY t.ts DESC
LIMIT 20;
```

If the first query returns no row, check the exact ioBroker object ID. If it
returns `type = 1` or `type = 2`, the state was stored as string or boolean and
is in `ts_string` or `ts_bool`; do not edit the dashboard query until the MQTT
payload and ioBroker type are confirmed.

## 2. Create a read-only Grafana database user

Use the actual ioBroker database name and the exact Grafana host. The example
assumes the database is named `iobroker`:

```sql
CREATE USER 'mot_grafana'@'grafana.internal.example'
  IDENTIFIED BY 'REPLACE_WITH_A_LONG_RANDOM_SECRET';

GRANT SELECT ON iobroker.*
  TO 'mot_grafana'@'grafana.internal.example';
```

Do not grant Grafana write, schema or user-administration privileges. If Grafana
runs on the same host as MySQL, the account may instead be restricted to
`'mot_grafana'@'localhost'`. Back up the database before unrelated administrative
changes; importing this dashboard itself performs no database writes.

## 3. Add the Grafana data source

In Grafana:

1. Open **Connections → Data sources**.
2. Select **MySQL**. This data source also supports MariaDB.
3. Enter the MySQL/MariaDB host and port.
4. Set **Database** to the ioBroker database, commonly `iobroker`.
5. Enter the read-only `mot_grafana` account.
6. Enable TLS when the database is on another host and configure the trusted CA.
7. Set the database session timezone to UTC where the Grafana version exposes
   that option.
8. Select **Save & test**.

Do not use the ioBroker database administrator or ioBroker writer account in
Grafana.

## 4. Import the dashboard

1. Download or copy
   [`mot-vehicle-overview.json`](../../tools/grafana/mot-vehicle-overview.json)
   from this repository. The smaller `mot-pioneer-soc.json` remains available as
   a query troubleshooting reference.
2. In Grafana, open **Dashboards → New → Import**.
3. Select **Upload dashboard JSON file** and choose the file.
4. In the import mapping, assign **ioBroker MySQL** to the data source created in
   the previous step.
5. Keep the proposed title or choose another folder/title.
6. Select **Import**.
7. Select `pioneer` in the **Fahrzeug** selector, set the time range to
   **Last 24 hours** and wait for the next MQTT update.

Import `mot-trips-energy.json` the same way for the trips and energy view. In
both dashboards, vehicle power uses the left axis and speed the right axis in a
combined chart. The trips dashboard uses Grafana's shared tooltip so hovering a
time chart shows the corresponding values at the same timestamp; its map tooltip
shows the route point's time, power and speed values.

The dashboard refreshes every 30 seconds. Grafana uses the browser timezone for
display; ioBroker timestamps remain Unix milliseconds. The imported panel sends
`t.ts DIV 1000` to Grafana as Unix seconds and therefore does not depend on the
MySQL session timezone.

## 5. Diagnose an empty panel

Run the two verification queries from step 1 in Grafana **Explore** using the
same data source. Then check:

- the exact object ID is `mqtt.2.mot.pioneer.display.soc`;
- SQL history is enabled for this object, not merely MQTT object creation;
- the samples are in `ts_number`;
- the selected Grafana time range contains the returned timestamps;
- the query returns at least one row; a single row is rendered as a visible
  point, while two or more rows form a line;
- the Grafana user can select from both `datapoints` and `ts_number`;
- the data source's default database is the ioBroker database;
- MySQL, the ioBroker host and Grafana agree on UTC/current time.

If the ioBroker tables use a non-default database name but the data source cannot
set it as default, qualify both tables in the panel query, for example
`iobroker.ts_number` and `iobroker.datapoints`.

## 6. Next panels

Once SOC works, duplicate the panel and replace only the exact data-point name
and unit. Suitable numeric states include:

| Object suffix | Grafana unit |
|---|---|
| `display/speed_kmh` | `km/h` |
| `display/odometer_km` | `km` |
| `bms/vehicle_power_w` | `watt` |
| `bms/pack_voltage` | `volt` |
| `bms/pack_current` | `ampere` |

Boolean charging and online signals live in `ts_bool` and require a separate
query. TeslaMate-style journeys and charging sessions require derived views or
tables and remain a later analytics work package.

The separate trips-and-energy dashboard integrates `bms/vehicle_power_w` only
across adjacent samples no more than 30 seconds apart. Consumption,
recuperation, charging and net-efficiency values are therefore bounded telemetry
estimates; missing intervals are excluded rather than interpolated. Its charging
panel groups measured minimum energy by UTC day. Exact individual charging
sessions and charging locations require a derived session model and, for places,
separately consented location history.

The energy queries use the SQL window function `LEAD()` and therefore require
MySQL 8.0 or MariaDB 10.2 or newer. Confirm the server with `SELECT VERSION();`
if an energy panel reports a SQL syntax error.

Enable SQL history for at least `display/odometer_km`, `display/speed_kmh`,
`bms/vehicle_power_w`, `bms/is_regenerating`, `charging/is_charging` and
`location/altitude_m` before
using this dashboard. Power must be stored at its regular MQTT cadence rather
than only after a large value change; otherwise every energy result is an
underestimate. Boolean states may be stored on change because the queries carry
the latest state forward. The dashboard deliberately rejects power intervals
longer than 30 seconds.

The route map additionally requires the separately owner-approved MQTT location
output and SQL history for `location/latitude` and `location/longitude`. It pairs
latitude, longitude, power and speed samples only when their receive timestamps
are within ten seconds. The native Grafana Geomap route layer draws forward
arrows in timestamp order and colors the route from the associated vehicle power:
blue below 0 kW, green from 0 kW, yellow from 3 kW and red from 10 kW. A broad
dashboard range can connect the last point of one journey to the first point of
another; select one journey's time window until a derived journey/session table
provides explicit route boundaries.

A separate, lightly rendered **Messpunkte** marker layer makes every stored GPS
sample individually interactive. Hovering shows its timestamp, coordinates,
power and speed; clicking the point keeps Grafana's detail tooltip open.

The map's current-position layer uses the newest GPS history row and renders the
Microlino SVG configured in section 7. A newly enabled ioBroker history state may
remain empty until the parked vehicle publishes a changed/new GPS value.

The performance chart shows consumption above zero and recuperation or charging
below zero; it overlays `display/speed_kmh` on a separate right-hand axis.
The speed query clamps invalid negative samples to zero, and only the right-hand
speed axis has a zero minimum; this does not limit the negative power axis.
Historical charging classification prefers `charging/is_charging`, then
`charging/plugged`, and finally standstill at no more than 1 km/h. This fallback
allows older negative charging-power samples to remain visible when Boolean
history was enabled later, but it is still telemetry classification rather than
a finalized charging-session record.

The altitude chart reads `location/altitude_m` and shares the dashboard time
range with the performance chart. It remains empty until ioBroker SQL history is
enabled for that state and a new value has been stored.

## 7. Use a Microlino image as the current-position marker

The supplied dashboard uses a Geomap **Photos** layer for the current vehicle
position. This renders the SVG as an image instead of treating it as a colored
marker mask. The image URL is returned as the `image_url` field by query B:

```text
https://www.microlino-open-telemetry.ch/images/microlino-outline.svg
```

The URL must remain reachable by every dashboard viewer.

To change the vehicle image in Grafana:

1. Edit the **Fahrtroute · Leistung und Fahrtrichtung** panel.
2. Open the **Aktueller Standort** Photos layer.
3. Keep **Image Source field** set to `image_url`.
4. Change the URL literal in query B and adjust **Radius** if needed.
5. Apply and save the panel.

Use an actual URL rather than a local filesystem path: the viewer's browser must
be able to load the image. The Photos layer also supports PNG, JPEG and Base64
image data.
