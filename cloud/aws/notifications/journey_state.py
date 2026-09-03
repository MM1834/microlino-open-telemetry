"""Pure journey detector and energy accumulator for JNY-001."""

from dataclasses import dataclass, replace
from typing import Optional


STOP_DELAY_MS = 10 * 60 * 1000
INACTIVITY_TIMEOUT_MS = 30 * 60 * 1000
MAX_CACHE_AGE_MS = 15 * 60 * 1000
MIN_POWER_SAMPLE_MS = 30 * 1000
MAX_POWER_GAP_MS = 90 * 1000
MIN_ODOMETER_SAMPLE_MS = 30 * 1000
MIN_MOVING_SAMPLE_MS = 60 * 1000
CHARGING_MOVEMENT_GUARD_MS = 2 * 60 * 1000
FIRMWARE_COUNTER_GRACE_MS = 10 * 60 * 1000
FIRMWARE_COUNTER_WAIT_MS = 10 * 60 * 1000
UNCHANGED_SOC_REFRESH_MS = 5 * 60 * 1000
MIN_DISTANCE_KM = 3.0
MIN_SOC_USED = 1.0
MIN_KWH_PER_100_KM = 1.0
MAX_KWH_PER_100_KM = 50.0


@dataclass(frozen=True)
class JourneyState:
    active_id: Optional[str] = None
    started_at: int = 0
    last_moving_at: int = 0
    stopped_at: int = 0
    stop_trigger: Optional[str] = None
    offline_at: int = 0
    start_odometer: Optional[float] = None
    last_odometer: Optional[float] = None
    start_soc: Optional[float] = None
    last_soc: Optional[float] = None
    odometer_valid: bool = True
    charging_observed: bool = False
    charging_after_stop: bool = False
    firmware_wait_until: int = 0
    estimated_drawn_kwh: float = 0.0
    estimated_regen_kwh: float = 0.0
    power_source: Optional[str] = None
    last_power_kw: Optional[float] = None
    last_power_at: int = 0
    bms_power_seen: bool = False
    firmware_counter_id: Optional[str] = None
    firmware_counter_invalid: bool = False
    firmware_drawn_wh: Optional[float] = None
    firmware_drawn_at: int = 0
    firmware_regen_wh: Optional[float] = None
    firmware_regen_at: int = 0
    firmware_net_wh: Optional[float] = None
    firmware_net_at: int = 0
    latest_soc: Optional[float] = None
    latest_soc_at: int = 0
    latest_odometer: Optional[float] = None
    latest_odometer_at: int = 0
    last_speed_at: int = 0
    last_online_at: int = 0
    latest_online: Optional[bool] = None
    last_completed_journey_id: Optional[str] = None
    last_completion_at: int = 0
    last_completion_reason: Optional[str] = None
    last_exclusion_reason: Optional[str] = None
    last_completion_trigger: Optional[str] = None


@dataclass(frozen=True)
class JourneySummary:
    journey_id: str
    started_at: int
    ended_at: int
    duration_minutes: int
    distance_km: float
    soc_used: float
    energy_drawn_kwh: float
    energy_regen_kwh: float
    energy_net_kwh: float
    net_kwh_per_100_km: float
    energy_source: str
    source_flag: str
    completion_trigger: str


def _is_number(value):
    return not isinstance(value, bool) and isinstance(value, (int, float))


def _fresh(value, updated_at, now):
    return value if updated_at and 0 <= now - updated_at <= MAX_CACHE_AGE_MS else None


def _start(state, received_at):
    return replace(
        state,
        active_id=str(received_at),
        started_at=received_at,
        last_moving_at=received_at,
        stopped_at=0,
        stop_trigger=None,
        offline_at=0,
        start_odometer=_fresh(state.latest_odometer, state.latest_odometer_at, received_at),
        last_odometer=_fresh(state.latest_odometer, state.latest_odometer_at, received_at),
        start_soc=_fresh(state.latest_soc, state.latest_soc_at, received_at),
        last_soc=_fresh(state.latest_soc, state.latest_soc_at, received_at),
        odometer_valid=True,
        charging_observed=False,
        charging_after_stop=False,
        firmware_wait_until=0,
        estimated_drawn_kwh=0.0,
        estimated_regen_kwh=0.0,
        power_source=None,
        last_power_kw=None,
        last_power_at=0,
        bms_power_seen=False,
        firmware_counter_id=None,
        firmware_counter_invalid=False,
        firmware_drawn_wh=None,
        firmware_drawn_at=0,
        firmware_regen_wh=None,
        firmware_regen_at=0,
        firmware_net_wh=None,
        firmware_net_at=0,
    )


def _apply_power(state, source, power_kw, received_at):
    if not state.active_id or received_at <= state.last_power_at:
        return state
    if source == "telemetry_display" and state.bms_power_seen:
        return state
    bms_seen = state.bms_power_seen or source == "telemetry_bms"
    if source != state.power_source:
        return replace(
            state, power_source=source, last_power_kw=power_kw,
            last_power_at=received_at, bms_power_seen=bms_seen,
        )
    if received_at - state.last_power_at < MIN_POWER_SAMPLE_MS:
        return state
    drawn = state.estimated_drawn_kwh
    regen = state.estimated_regen_kwh
    elapsed = received_at - state.last_power_at
    if state.last_power_kw is not None and 0 < elapsed <= MAX_POWER_GAP_MS:
        energy = ((state.last_power_kw + power_kw) / 2.0) * elapsed / 3_600_000.0
        if energy >= 0:
            drawn += energy
        else:
            regen += -energy
    return replace(
        state, estimated_drawn_kwh=drawn, estimated_regen_kwh=regen,
        last_power_kw=power_kw, last_power_at=received_at,
        bms_power_seen=bms_seen,
    )


def apply_journey_update(state: JourneyState, suffix: str, value, received_at: int):
    """Apply one telemetry sample without allowing replayed data to rewind state."""
    if received_at <= 0:
        return state
    if (
        suffix != "status/online" and state.offline_at
        and received_at > state.offline_at
    ):
        state = replace(state, offline_at=0, latest_online=True)
    if suffix == "display/soc":
        if not _is_number(value) or not 0 <= value <= 100 or received_at <= state.latest_soc_at:
            return state
        current = float(value)
        if current == state.latest_soc and received_at - state.latest_soc_at < UNCHANGED_SOC_REFRESH_MS:
            return state
        updates = {"latest_soc": current, "latest_soc_at": received_at}
        if state.active_id:
            updates["last_soc"] = current
            if state.start_soc is None:
                updates["start_soc"] = current
        return replace(state, **updates)
    if suffix == "display/odometer_km":
        if not _is_number(value) or value < 0 or received_at <= state.latest_odometer_at:
            return state
        current = float(value)
        if (
            state.latest_odometer is not None
            and current - state.latest_odometer < 0.5
            and received_at - state.latest_odometer_at < MIN_ODOMETER_SAMPLE_MS
        ):
            return state
        updates = {"latest_odometer": current, "latest_odometer_at": received_at}
        if state.active_id:
            updates["odometer_valid"] = (
                state.odometer_valid
                and (state.last_odometer is None or current >= state.last_odometer)
            )
            updates["last_odometer"] = current
            if state.start_odometer is None:
                updates["start_odometer"] = current
        return replace(state, **updates)
    if suffix == "display/speed_kmh":
        if not _is_number(value) or value < 0 or received_at <= state.last_speed_at:
            return state
        updated = replace(state, last_speed_at=received_at)
        if float(value) > 1.0:
            # A confirmed charge/plug signal seals the preceding journey. Ignore
            # later speed noise until the sealed state has been finalized.
            if updated.active_id and updated.charging_after_stop:
                return updated
            updated = _start(updated, received_at) if not updated.active_id else updated
            if (
                updated.active_id == state.active_id and not state.stopped_at
                and received_at - state.last_speed_at < MIN_MOVING_SAMPLE_MS
            ):
                return state
            return replace(
                updated,
                last_moving_at=received_at,
                stopped_at=0,
                stop_trigger=None,
                charging_observed=updated.charging_observed,
                charging_after_stop=False,
            )
        if updated.active_id and not updated.stopped_at:
            return replace(
                updated, stopped_at=received_at, stop_trigger="speed_zero"
            )
        return updated
    if suffix == "status/online":
        if not isinstance(value, bool) or received_at <= state.last_online_at:
            return state
        if value == state.latest_online:
            return state
        return replace(
            state, last_online_at=received_at, latest_online=value,
            offline_at=(0 if value else received_at),
        )
    if suffix in {"charging/plugged", "charging/is_charging"}:
        if isinstance(value, bool) and value and state.active_id:
            # A vehicle cannot physically begin charging while it is moving.
            # Ignore isolated plug/charge assertions until either speed zero was
            # observed or movement has been absent for a bounded interval. A
            # real charge continues publishing and is then accepted.
            if (
                not state.stopped_at and state.last_moving_at
                and 0 <= received_at - state.last_moving_at
                < CHARGING_MOVEMENT_GUARD_MS
            ):
                return state
            if state.charging_after_stop:
                return state
            # Standard-CAN charging is a hard boundary, not activity inside a
            # drive. Preserve an earlier zero-speed stop when present; otherwise
            # seal at the charge observation using the last moving endpoints.
            return replace(
                state,
                stopped_at=(state.stopped_at or received_at),
                stop_trigger=(state.stop_trigger or "standard_can_charging"),
                charging_after_stop=True,
                firmware_wait_until=received_at + FIRMWARE_COUNTER_GRACE_MS,
            )
        return state
    if suffix == "bms/vehicle_power_w":
        if not _is_number(value):
            return state
        return _apply_power(state, "telemetry_bms", float(value) / 1000.0, received_at)
    if suffix == "charging/power_signed":
        if not _is_number(value):
            return state
        return _apply_power(state, "telemetry_display", float(value) / 10.0, received_at)
    if suffix == "journey/energy_counter_id":
        if not state.active_id or not isinstance(value, (str, int)):
            return state
        counter_id = str(value)[:80]
        if state.firmware_counter_id and counter_id != state.firmware_counter_id:
            return replace(state, firmware_counter_invalid=True)
        return replace(state, firmware_counter_id=counter_id)
    firmware_fields = {
        "journey/energy_drawn_wh": "firmware_drawn_wh",
        "journey/energy_regen_wh": "firmware_regen_wh",
        "journey/energy_net_wh": "firmware_net_wh",
    }
    field = firmware_fields.get(suffix)
    if field and state.active_id and _is_number(value) and value >= 0:
        previous = getattr(state, field)
        if previous is not None and float(value) < previous:
            return replace(state, firmware_counter_invalid=True)
        return replace(state, **{field: float(value), field.replace("_wh", "_at"): received_at})
    return state


def summarize_journey(
    state: JourneyState,
    now_ms: int,
    finalize_stable_stop: bool = False,
):
    """Return (summary, reason) once an active journey has been stopped for 10 min."""
    if (
        not state.active_id
        or not state.stopped_at
        or (
            not state.charging_after_stop
            and state.stop_trigger != "telemetry_timeout"
            and now_ms - state.stopped_at < STOP_DELAY_MS
        )
    ):
        return None, "not_due"
    if state.charging_observed:
        return None, "charging_observed"
    if not state.odometer_valid or state.start_odometer is None or state.last_odometer is None:
        return None, "invalid_odometer"
    if state.start_soc is None or state.last_soc is None:
        return None, "missing_soc"
    distance = state.last_odometer - state.start_odometer
    soc_used = state.start_soc - state.last_soc
    if distance < MIN_DISTANCE_KM:
        return None, "distance_too_short"
    if soc_used < MIN_SOC_USED:
        return None, "soc_drop_too_small"

    firmware_net = state.firmware_net_wh
    firmware_counter_fresh = state.firmware_net_at >= state.last_moving_at
    if firmware_net is None and state.firmware_drawn_wh is not None and state.firmware_regen_wh is not None:
        firmware_net = state.firmware_drawn_wh - state.firmware_regen_wh
        firmware_counter_fresh = min(
            state.firmware_drawn_at, state.firmware_regen_at,
        ) >= state.last_moving_at
    firmware_counter_ready = (
        state.firmware_counter_id and not state.firmware_counter_invalid
        and firmware_net is not None and firmware_counter_fresh
    )
    if (
        state.charging_after_stop and not firmware_counter_ready
        and now_ms < state.firmware_wait_until
    ):
        return None, "not_due"
    if (
        not state.charging_after_stop
        and state.stop_trigger != "telemetry_timeout"
        and not firmware_counter_ready
        and not finalize_stable_stop
        and now_ms < state.stopped_at + STOP_DELAY_MS + FIRMWARE_COUNTER_WAIT_MS
    ):
        return None, "not_due"
    if (
        firmware_counter_ready
    ):
        drawn = (state.firmware_drawn_wh or max(firmware_net, 0.0)) / 1000.0
        regen = (state.firmware_regen_wh or max(-firmware_net, 0.0)) / 1000.0
        net = firmware_net / 1000.0
        source = "firmware_counter"
        flag = "Firmware-Zähler"
    else:
        drawn = state.estimated_drawn_kwh
        regen = state.estimated_regen_kwh
        net = drawn - regen
        source = "telemetry_estimate"
        flag = "Telemetrie-Schätzung"
    rate = net / distance * 100.0
    if net <= 0 or not MIN_KWH_PER_100_KM <= rate <= MAX_KWH_PER_100_KM:
        return None, "implausible_energy"
    duration = max(1, round((state.last_moving_at - state.started_at) / 60_000.0))
    return JourneySummary(
        journey_id=state.active_id, started_at=state.started_at,
        ended_at=state.stopped_at, duration_minutes=duration,
        distance_km=distance, soc_used=soc_used, energy_drawn_kwh=drawn,
        energy_regen_kwh=regen, energy_net_kwh=net,
        net_kwh_per_100_km=rate, energy_source=source, source_flag=flag,
        completion_trigger=(state.stop_trigger or "stable_stop"),
    ), "eligible"


def apply_inactivity_timeout(
    state: JourneyState,
    now_ms: int,
    timeout_ms: int = INACTIVITY_TIMEOUT_MS,
):
    """Seal an active journey after 30 minutes without relevant telemetry."""
    if not state.active_id or state.stopped_at or now_ms <= 0:
        return state
    last_signal_at = max(
        state.last_moving_at, state.last_power_at, state.latest_odometer_at,
        state.latest_soc_at, state.last_speed_at,
    )
    if not last_signal_at or now_ms - last_signal_at < timeout_ms:
        return state
    return replace(
        state, stopped_at=last_signal_at, stop_trigger="telemetry_timeout"
    )


def clear_journey(
    state: JourneyState,
    reason: Optional[str] = None,
    completed_at: int = 0,
):
    """Clear active fields while retaining endpoint and completion diagnostics."""
    completion_reason = reason or state.last_completion_reason
    exclusion_reason = (
        None if completion_reason == "eligible"
        else completion_reason or state.last_exclusion_reason
    )
    return JourneyState(
        latest_soc=state.latest_soc, latest_soc_at=state.latest_soc_at,
        latest_odometer=state.latest_odometer,
        latest_odometer_at=state.latest_odometer_at,
        last_speed_at=state.last_speed_at, last_online_at=state.last_online_at,
        latest_online=state.latest_online,
        last_completed_journey_id=(state.active_id or state.last_completed_journey_id),
        last_completion_at=(completed_at or state.last_completion_at),
        last_completion_reason=completion_reason,
        last_exclusion_reason=exclusion_reason,
        last_completion_trigger=(
            state.stop_trigger or state.last_completion_trigger
        ),
    )
