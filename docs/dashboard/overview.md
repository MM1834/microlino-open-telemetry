# Dashboard Overview

![Desktop Home](../assets/images/dashboard/desktop-home.png)

The dashboard provides current and live telemetry through the configured provider.
German remains the project language and the portal's default and fallback. A
persisted dashboard selector additionally supports English, French and Italian. It applies
to static content, runtime status and error messages, accessibility labels,
locale-sensitive dates and History chart annotations. The local firmware wizard
is a separate surface and remains English-only.

The adaptive charging/power overview card evaluates the authoritative timestamp
of the topics used by its current mode. After the existing 120-second freshness
boundary, the retained charging or power presentation is dimmed and annotated
with `Nicht aktuell · letzter Messpunkt hh:mm`; a fresh update clears the marker.
The net-power History card uses the same current/stale wording. Its timestamp is
derived from the last real power sample rather than synthetic zeroes used only to
close inactive chart gaps.
For the AWS backend it renders authorized SOC, Speed, signed-power, charging and
plugged history for 24 hours, 7 days and 30 days. Speed and power gaps are closed
at zero in the chart rather than linearly interpolated across missing reception.
Legacy MQTT retains browser-local history as a fallback.
AWS History is loaded on entry, range selection and actual vehicle changes rather
than on the five-second live-state poll. Concurrent same-range requests are
coalesced, stale responses are ignored, and a transient API failure leaves the
last successfully rendered charts visible with a warning.

The overview range card uses an automatic personal forecast when the backend has
enough valid distance/SOC evidence. The vehicle card explains the basis as driven
kilometres and journey count and retains the fixed `SOC × configured 100% range`
result as a comparison. Before valid history exists, the fixed value remains the
only displayed forecast. Both `/dashboard/` and `/motbeta/` consume this shared
portal source.

The dedicated journey page additionally shows the current or retained-last
journey distance and the newest real odometer. Its separate `Seit letzter Ladung`
card uses the SOC and odometer retained when the last qualified charging session
ended. The same card now also shows the measured energy of that completed charge,
its power-data coverage and the separate SOC estimate when available. Coverage
below 95 percent labels the measured value as a minimum, matching the charging
email. It displays observed distance and SOC development immediately but only
projects range to zero and the configured reserve after at least 5 km and 5 SOC
percentage points. Missing or inconsistent values remain unavailable rather than
being inferred. This short-window observation is deliberately separate from the
long-term personal range forecast.

The main dashboard's charging card uses that same protected last-charge record in
its existing energy field. It shows measured energy, marks incomplete coverage as
`mind.`, and adds the optional SOC estimate, coverage and completion time as
secondary text. It refreshes once per minute and clears the previous value before
a vehicle change. This is completed standard notification telemetry, not live
energy metering and not diagnostic-history data.

CHG-BAL-001 evolves that record into a dashboard-only balance between drives.
Individual charging-summary emails remain bounded per qualified session, while
the display aggregates repeated charges, top-ups and measurable discharge until
real movement. It shows gross charged energy as the primary value and adds net
battery input, discharge, SOC progression, session count, coverage and open/final
state when available. Legacy single-session records retain their previous shape.

The Vehicle card also contains an authenticated previous-month efficiency
comparison. It shows the selected vehicle's stored net kWh/100 km beside the
anonymous community value after subtracting that vehicle's contribution. A `+`
means at least five percent lower consumption, `=` is the five-percent neutral
band and `-` means at least five percent higher consumption. The result is hidden
behind an insufficient-data state unless at least three other vehicles, ten
journeys and 100 km remain. SOC and battery capacity are never part of this API
or card. German, English, French and Italian wording is included. Hosted desktop
and smartphone acceptance passed on 2026-09-10, including coexistence with the
separate DRV-001 live-journey page.

The three compact Vehicle-card values use a separate short-term period beginning
at the latest qualified charging completion. `Trip` is the real odometer delta,
`Verbrauch` is accumulated net traction energy per 100 km, and `Fahrzeit` sums
the finalized journey durations. The protected current-journey response adds the
active journey provisionally without persisting or double-counting it. A newer
charge reference immediately invalidates older totals; legacy records remain
unavailable until later charge-and-drive evidence exists. Dashboard kilometre
values are rounded to whole kilometres because the retained display odometer
does not provide reliable 100 m resolution; calculations and qualification
retain their available internal precision.

The authenticated Settings page ends with a read-only `MOT Adapter
Informationen` block for the selected vehicle. It obtains DeviceId, Board,
firmware version, CAN1/CAN2 decoder profile keys and the adapter's local IP
address from the existing authorized snapshot. Missing retained topics from
older firmware show `--`; REV18 publishes Board as `system/board`. Changing the
Settings vehicle reloads both preferences and adapter information without
starting live polling or a WebSocket.

The backend derives the forecast from at most the ten newest valid journeys in
the last 30 days and stops adding older journeys after about 150 km. Charging,
odometer resets, implausible efficiency and very small segments are excluded.
Up to 100 km and 20 consumed SOC percentage points, the historical kilometres per
SOC point are progressively blended with the user's configured full-range basis;
beyond both evidence thresholds the personal value is used without baseline
weighting. The authenticated per-user/per-vehicle settings default to 140 km at
100% SOC and 0% reserve. A non-zero reserve is subtracted from current SOC for
both the fixed comparison and personal forecast, so the displayed usable range
reaches zero at the chosen reserve and is labelled `bis N%`.

The repository portal also renders the Standard-CAN BMS topics
`bms/pack_voltage`, `bms/pack_current`, `bms/pack_power_w` and the provisional
`bms/cell_min_mv`, `bms/cell_max_mv` and `bms/cell_delta_mv` values. The generic
AWS state and WebSocket path requires no schema deployment for these live fields.
This repository support does not mean the updated static portal package has been
uploaded to the hosted site. The portal labels the two `0x4AD` values as
cell-voltage candidates because their status as true pack-wide extrema is not yet
independently confirmed.

Following the controlled Pioneer road test, the portal also understands
`bms/vehicle_power_w`, `bms/is_regenerating` and `bms/is_discharging`. The battery
card uses consumption-positive vehicle power while raw pack power retains the
battery convention (positive into the pack). This prevents charging and
regeneration from being displayed with the traction sign.

On smartphone layouts, the leading charging card becomes a driving energy card
above 1 km/h. It then shows the same `Verbrauch`, `Rekuperation` or `Bereit`
power-flow state and vehicle-power magnitude as the battery detail card. At
standstill it continues to show `Nicht am Laden`, `Eingesteckt` or `Lädt`; desktop
layouts retain the charging card at all speeds.

The smartphone energy card adds a five-pixel live-power bar without increasing
the card height. Consumption uses a 0–20 kW scale with green through 3 kW, amber
through 10 kW and red above 10 kW. Regeneration also uses 0–20 kW, progressing
from light green through 5 kW to green through 10 kW and dark green above it.
Charging uses a finer 0–3.5 kW scale with light green through 1.6 kW, green through
2.4 kW and dark green above it. The bar appears only while charging or moving;
the numeric value and flow label remain the authoritative reading.

The live battery card presents charging power as a positive magnitude and changes
its label to `Ladeleistung` while charging. History uses a vehicle-facing signed
display: consumption is negative, while charging and regeneration are positive.
The chart uses a symmetric Y-axis and emphasized zero line and labels the newest
point with its power-flow direction. Consumption and regeneration within one
aggregation interval can partly cancel. The underlying signed vehicle-power topic
and stored History value remain positive for vehicle consumption and negative for
energy entering the battery; only the portal representation is inverted. Existing
records require no migration. The Speed chart marks its newest
measurement as not current after the expected sampling interval; this can mean
either standstill suppression or an offline device and is not by itself a
connectivity diagnosis.

Charging and plugged History share one binary chart. Charging is a solid purple
step line and cable connection a dashed pink step line. Each reported state is
held horizontally until its next reported state and changes only through a
vertical edge at that timestamp. Missing samples therefore never create a
misleading diagonal transition, and the two independently reported values remain
visible when they overlap.

On desktop, Battery and Vehicle use equal 180-pixel central instruments. Battery
places voltage, current, vehicle/charging power and power flow in a bounded 2×2
grid below its SoC ring, matching the Vehicle speedometer-plus-summary hierarchy
and preventing either instrument from crossing its card boundary. Smartphone
detail-card layout is unchanged by this desktop-only rule.

## Main views
- Home
- Battery
- Vehicle
- Charging
- Temperatures
- Cells
- Location

On touch-oriented input devices (`pointer: coarse` or `hover: none`), the
embedded OpenStreetMap is non-interactive by default so vertical gestures
continue scrolling the dashboard. This is independent of the responsive layout
and therefore also covers tablets such as an iPad that render the desktop view.
The explicit `Karte bedienen` control enables map pan and zoom; the same control
switches back to page scrolling, and touching outside the map also leaves
interaction mode. Mouse/trackpad interaction remains immediate at every viewport
width, while the external OpenStreetMap link is always available.

## Administration

Privileged beta-onboarding claim issuance and Web-Flasher grant/revoke controls
live on the separate `/dashboard/administration/` page. The Administration page
also contains separate read-only inventories of currently
valid WebFlash and password-recovery grants. The server filters expiry and exact
release compatibility, resolves user email through Cognito and returns the list
only to `mot-beta-admins`; it refreshes after every grant or revoke operation.
The dashboard exposes its Administration navigation entry only when the restored Cognito access token
contains `mot-beta-admins`; the destination keeps all forms hidden until it has
independently checked the same claim. Signed-out and non-administrator users see
no privileged controls. The onboarding and firmware APIs remain the authoritative
server-side authorization boundary.

## Settings

Personal, vehicle-specific range and notification controls live on the dedicated
authenticated `/dashboard/settings/` page. It lists only the signed-in user's
assigned vehicles and scopes each preference read and write to the selected
vehicle. The page does not start telemetry polling or a WebSocket connection.
The main dashboard retains only a background preference read for full-range and
SOC-reserve values because those two settings directly affect its range forecast.

## Journey email preference

The optional daily summary is independent from individual journey and charging
emails. It totals completed events for the preceding `Europe/Zurich` calendar day,
waits hourly for an active session until 08:05 and sends no email on an empty day.
Sessions ending after midnight belong to their completion day rather than being
split at midnight.

The notification settings include a separate, default-off opt-in for qualifying
journey summaries. It reuses the configured email channel and cannot be enabled
without that channel. The repository UI describes delivery for qualifying
journeys and notes that each email identifies either `Telemetrie-Schätzung` or
`Firmware-Zähler` as its energy source. Existing devices use the estimate path;
future firmware counters can take priority without changing the preference.

Journey completion normally follows ten minutes of stable standstill. For the
accepted Pioneer Standard-CAN decoder, a confirmed plug or charging signal is a
hard boundary that immediately seals the preceding drive; later legacy speed
noise cannot reopen it. The latest per-vehicle completion decision and exclusion
reason remain in backend diagnostics after active journey state is cleared.

If coverage disappears before the final speed or charging signal reaches AWS,
the backend finalizes the journey after 30 minutes without relevant telemetry.
It uses the last received signal as the endpoint and labels the email as a
telemetry timeout, so unobserved distance inside a garage is not presented as
measured journey data.

## SMS destination preference

The repository portal supports a controlled Swiss and German SMS destination lifecycle. A
user enters a `+41` or `+49` mobile number for the selected vehicle, requests and confirms
the AWS verification code, and can enable SMS only after a separate administrator
approval matches that user, vehicle and number. The same verified number may be
associated with several vehicles or users; verification is shared by number,
while approval and opt-in remain separate for every user–vehicle association.
The portal never lists the other associations.

Changing the number disables SMS until the new exact destination is verified and
approved. Portal state is not a delivery authorization: the backend dispatcher
must still enforce every SMS-001 spend, alarm, sender, country, rate and kill
switch at send time. The portal implementation is not yet evidence of hosted or
handset acceptance.

The verification-code input is shown only after the user requests a code and
while that verification remains pending. A confirmed number therefore presents
only its verification/approval state and vehicle-specific SMS opt-in. The save
path captures the opt-in before switching the form into its busy presentation so
that stale status cannot clear the selected checkbox.

## Charging-stop session boundary

Charging-stop notification is intentionally limited to one event per continuous
plugged session. Unplugging and plugging in again starts a new session. Merely
resuming charging while the vehicle remains connected does not reset the
idempotency boundary.

This avoids repeated warnings when a Microlino remains connected for hours or
days and intermittently tops up. The same rule covers external control such as
solar zero-export operation, load management or a manual pause: the first
qualified stop may notify, while later stop/resume cycles in the same plugged
session remain suppressed. SOC-target notification remains an independent event.
