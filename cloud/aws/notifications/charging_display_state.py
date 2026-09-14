"""Pure state machine for the dashboard charge balance between journeys."""

from dataclasses import dataclass, replace
from typing import Optional


START_QUALIFICATION_MS = 45_000
MAX_POWER_GAP_MS = 30_000
MOVEMENT_CONFIRMATION_MS = 60_000
ODOMETER_MOVEMENT_KM = 0.05


@dataclass(frozen=True)
class ChargingDisplayState:
    plugged: bool = False
    is_charging: Optional[bool] = None
    candidate_at: int = 0
    candidate_start_soc: Optional[float] = None
    candidate_peak_soc: Optional[float] = None
    candidate_gross_kwh: float = 0.0
    candidate_discharged_kwh: float = 0.0
    candidate_covered_ms: int = 0
    candidate_largest_gap_ms: int = 0
    candidate_sample_count: int = 0
    last_soc: Optional[float] = None
    last_soc_at: int = 0
    last_odometer: Optional[float] = None
    last_odometer_at: int = 0
    block_open: bool = False
    block_started_at: int = 0
    block_start_soc: Optional[float] = None
    block_peak_soc: Optional[float] = None
    block_gross_kwh: float = 0.0
    block_discharged_kwh: float = 0.0
    block_covered_ms: int = 0
    block_largest_gap_ms: int = 0
    block_sample_count: int = 0
    block_session_count: int = 0
    block_capacity_kwh: Optional[float] = None
    block_last_charge_at: int = 0
    block_reference_odometer: Optional[float] = None
    motion_candidate_at: int = 0
    odometer_moved: bool = False
    last_power_w: float = 0.0
    last_power_at: int = 0
    power_source: Optional[str] = None
    last_energy_kwh: Optional[float] = None
    last_discharged_kwh: Optional[float] = None
    last_net_kwh: Optional[float] = None
    last_soc_estimate_kwh: Optional[float] = None
    last_coverage_percent: Optional[float] = None
    last_start_soc: Optional[float] = None
    last_peak_soc: Optional[float] = None
    last_end_soc: Optional[float] = None
    last_session_count: int = 0
    last_charge_at: int = 0
    last_finalized_at: int = 0
    last_reference_soc: Optional[float] = None
    last_reference_odometer: Optional[float] = None


def _is_number(value):
    return not isinstance(value, bool) and isinstance(value, (int, float))


def _reset_candidate(state):
    return replace(
        state,
        candidate_at=0,
        candidate_start_soc=None,
        candidate_peak_soc=None,
        candidate_gross_kwh=0.0,
        candidate_discharged_kwh=0.0,
        candidate_covered_ms=0,
        candidate_largest_gap_ms=0,
        candidate_sample_count=0,
        last_power_at=0,
        power_source=None,
    )


def _open_block(state, at):
    return replace(
        state,
        block_open=True,
        block_started_at=(state.candidate_at or at),
        block_start_soc=state.candidate_start_soc,
        block_peak_soc=state.candidate_peak_soc,
        block_gross_kwh=state.candidate_gross_kwh,
        block_discharged_kwh=state.candidate_discharged_kwh,
        block_covered_ms=state.candidate_covered_ms,
        block_largest_gap_ms=state.candidate_largest_gap_ms,
        block_sample_count=state.candidate_sample_count,
        block_session_count=1,
        candidate_at=0,
        candidate_start_soc=None,
        candidate_peak_soc=None,
        candidate_gross_kwh=0.0,
        candidate_discharged_kwh=0.0,
        candidate_covered_ms=0,
        candidate_largest_gap_ms=0,
        candidate_sample_count=0,
    )


def _integrate(state, source, watts, at):
    if state.power_source == "vehicle_power_w" and source != "vehicle_power_w":
        return state
    prefix = "block" if state.block_open else "candidate"
    if source != state.power_source:
        samples = getattr(state, f"{prefix}_sample_count") + 1
        return replace(state, power_source=source, last_power_w=watts,
                       last_power_at=at, **{f"{prefix}_sample_count": samples})
    gross = state.block_gross_kwh if state.block_open else state.candidate_gross_kwh
    discharged = (
        state.block_discharged_kwh if state.block_open
        else state.candidate_discharged_kwh
    )
    covered = state.block_covered_ms if state.block_open else state.candidate_covered_ms
    largest = (
        state.block_largest_gap_ms if state.block_open
        else state.candidate_largest_gap_ms
    )
    samples = (
        state.block_sample_count if state.block_open
        else state.candidate_sample_count
    ) + 1
    if state.last_power_at:
        gap = at - state.last_power_at
        if gap > 0:
            largest = max(largest, gap)
        if 0 < gap <= MAX_POWER_GAP_MS:
            previous = state.last_power_w
            if previous * watts < 0:
                first_ms = gap * abs(previous) / (abs(previous) + abs(watts))
                first = abs(previous) * first_ms / 2.0 / 3_600_000_000.0
                second = abs(watts) * (gap - first_ms) / 2.0 / 3_600_000_000.0
                if previous < 0:
                    gross += first
                    discharged += second
                else:
                    discharged += first
                    gross += second
            else:
                energy = ((previous + watts) / 2.0) * gap / 3_600_000_000.0
                if energy < 0:
                    gross += -energy
                else:
                    discharged += energy
            covered += gap
    updates = {
        "last_power_w": watts,
        "last_power_at": at,
        "power_source": source,
    }
    updates.update({
        f"{prefix}_gross_kwh": gross,
        f"{prefix}_discharged_kwh": discharged,
        f"{prefix}_covered_ms": covered,
        f"{prefix}_largest_gap_ms": largest,
        f"{prefix}_sample_count": samples,
    })
    return replace(state, **updates)


def snapshot(state, at, finalized=False):
    """Return the current/final balance without changing email session state."""
    if not state.block_open:
        return state
    duration = max(1, at - state.block_started_at)
    coverage = min(100.0, max(0.0, state.block_covered_ms * 100.0 / duration))
    soc_delta = (
        state.last_soc - state.block_start_soc
        if state.last_soc is not None and state.block_start_soc is not None
        else None
    )
    estimate = (
        state.block_capacity_kwh * soc_delta / 100.0
        if state.block_capacity_kwh is not None and soc_delta is not None
        and soc_delta >= 0 else None
    )
    return replace(
        state,
        last_energy_kwh=state.block_gross_kwh,
        last_discharged_kwh=state.block_discharged_kwh,
        last_net_kwh=state.block_gross_kwh - state.block_discharged_kwh,
        last_soc_estimate_kwh=estimate,
        last_coverage_percent=coverage,
        last_start_soc=state.block_start_soc,
        last_peak_soc=state.block_peak_soc,
        last_end_soc=state.last_soc,
        last_session_count=state.block_session_count,
        last_charge_at=(state.block_last_charge_at or at),
        last_finalized_at=(at if finalized else 0),
        last_reference_soc=(state.last_soc if finalized else state.last_reference_soc),
        last_reference_odometer=(
            state.block_reference_odometer if finalized
            else state.last_reference_odometer
        ),
    )


def finalize(state, at):
    if not state.block_open:
        return state
    state = snapshot(state, at, finalized=True)
    return replace(
        state,
        block_open=False,
        block_started_at=0,
        block_start_soc=None,
        block_peak_soc=None,
        block_gross_kwh=0.0,
        block_discharged_kwh=0.0,
        block_covered_ms=0,
        block_largest_gap_ms=0,
        block_sample_count=0,
        block_session_count=0,
        block_capacity_kwh=None,
        block_last_charge_at=0,
        block_reference_odometer=None,
        motion_candidate_at=0,
        odometer_moved=False,
        last_power_at=0,
        power_source=None,
    )


def attach_capacity(state, capacity_kwh):
    if (
        not state.block_open or not _is_number(capacity_kwh)
        or float(capacity_kwh) <= 0
    ):
        return state
    return replace(state, block_capacity_kwh=float(capacity_kwh))


def apply(state, suffix, value, at):
    if at <= 0:
        return state
    if suffix == "display/soc":
        if not _is_number(value) or not 0 <= value <= 100 or at <= state.last_soc_at:
            return state
        peak = state.block_peak_soc
        candidate_peak = state.candidate_peak_soc
        if state.block_open:
            peak = float(value) if peak is None else max(peak, float(value))
        elif state.candidate_at:
            candidate_peak = (
                float(value) if candidate_peak is None
                else max(candidate_peak, float(value))
            )
        return replace(
            state, last_soc=float(value), last_soc_at=at,
            block_peak_soc=peak, candidate_peak_soc=candidate_peak,
        )
    if suffix in {"display/odometer_km", "display/odo"}:
        if not _is_number(value) or value < 0 or at <= state.last_odometer_at:
            return state
        current = float(value)
        state = replace(state, last_odometer=current, last_odometer_at=at)
        if (
            state.block_open and state.block_reference_odometer is not None
            and current >= state.block_reference_odometer + ODOMETER_MOVEMENT_KM
        ):
            candidate = state.motion_candidate_at or at
            state = replace(
                state, motion_candidate_at=candidate, odometer_moved=True
            )
            return (
                finalize(state, at)
                if at - candidate >= MOVEMENT_CONFIRMATION_MS else state
            )
        return state
    if suffix == "charging/plugged" and isinstance(value, bool):
        state = replace(state, plugged=value)
        if not value:
            if state.block_open and state.is_charging is True:
                state = replace(
                    state,
                    block_last_charge_at=at,
                    block_reference_odometer=state.last_odometer,
                )
                state = snapshot(state, at)
            state = replace(state, is_charging=False, last_power_at=0, power_source=None)
            if not state.block_open:
                state = _reset_candidate(state)
        return state
    if suffix == "charging/is_charging" and isinstance(value, bool):
        if value:
            if state.block_open:
                sessions = state.block_session_count + (state.is_charging is not True)
                return replace(
                    state, is_charging=True, block_session_count=sessions,
                    motion_candidate_at=0, odometer_moved=False,
                )
            candidate = state.candidate_at or at
            state = replace(
                state,
                is_charging=True,
                candidate_at=candidate,
                candidate_start_soc=(
                    state.candidate_start_soc
                    if state.candidate_start_soc is not None else state.last_soc
                ),
                candidate_peak_soc=(
                    state.candidate_peak_soc
                    if state.candidate_peak_soc is not None else state.last_soc
                ),
            )
            return _open_block(state, at) if at - candidate >= START_QUALIFICATION_MS else state
        state = replace(state, is_charging=False, last_power_at=0, power_source=None)
        if state.block_open:
            state = replace(
                state,
                block_last_charge_at=at,
                block_reference_odometer=state.last_odometer,
            )
            return snapshot(state, at)
        return _reset_candidate(state)
    if suffix == "display/speed_kmh":
        if not _is_number(value) or value < 0 or not state.block_open:
            return state
        if float(value) <= 1.0:
            return state if state.odometer_moved else replace(
                state, motion_candidate_at=0
            )
        candidate = state.motion_candidate_at or at
        state = replace(state, motion_candidate_at=candidate)
        return finalize(state, at) if at - candidate >= MOVEMENT_CONFIRMATION_MS else state
    if suffix not in {"bms/vehicle_power_w", "charging/power_signed"}:
        return state
    if not _is_number(value) or not (state.block_open or state.candidate_at):
        return state
    source = "vehicle_power_w" if suffix == "bms/vehicle_power_w" else "power_signed"
    watts = float(value) if source == "vehicle_power_w" else float(value) * 100.0
    return _integrate(state, source, watts, at)
