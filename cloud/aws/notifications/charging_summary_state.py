"""Pure state machine for optional email-only charging summaries."""

from dataclasses import dataclass, replace
from typing import Optional

START_QUALIFICATION_MS = 45_000
SUMMARY_DELAY_MS = 10 * 60_000
MAX_POWER_GAP_MS = 30_000
ODOMETER_REFERENCE_MAX_AGE_MS = 30 * 60_000


@dataclass(frozen=True)
class ChargingSummaryState:
    session_id: Optional[str] = None
    plugged: bool = False
    is_charging: Optional[bool] = None
    candidate_at: int = 0
    active: bool = False
    started_at: int = 0
    start_soc: Optional[float] = None
    last_soc: Optional[float] = None
    last_soc_at: int = 0
    last_odometer: Optional[float] = None
    last_odometer_at: int = 0
    last_charge_soc: Optional[float] = None
    last_charge_odometer: Optional[float] = None
    last_charge_at: int = 0
    last_charge_energy_kwh: Optional[float] = None
    last_charge_energy_at: int = 0
    last_charge_coverage_percent: Optional[float] = None
    last_charge_soc_estimate_kwh: Optional[float] = None
    last_charge_start_soc: Optional[float] = None
    last_charge_end_soc: Optional[float] = None
    stop_candidate_at: int = 0
    energy_kwh: float = 0.0
    last_power_w: float = 0.0
    last_power_at: int = 0
    power_source: Optional[str] = None
    covered_power_ms: int = 0
    largest_power_gap_ms: int = 0
    power_sample_count: int = 0


def apply(state, suffix, value, at):
    if at <= 0:
        return state
    if suffix == "display/soc" and not isinstance(value, bool) and isinstance(value, (int, float)):
        if at > state.last_soc_at and 0 <= value <= 100:
            return replace(state, last_soc=float(value), last_soc_at=at)
        return state
    if suffix in {"display/odometer_km", "display/odo"} and not isinstance(value, bool) and isinstance(value, (int, float)):
        if at > state.last_odometer_at and value >= 0:
            return replace(state, last_odometer=float(value), last_odometer_at=at)
        return state
    if suffix == "charging/plugged" and isinstance(value, bool):
        if value and not state.plugged:
            return ChargingSummaryState(session_id=str(at), plugged=True,
                                        last_soc=state.last_soc, last_soc_at=state.last_soc_at,
                                        last_odometer=state.last_odometer,
                                        last_odometer_at=state.last_odometer_at,
                                        last_charge_soc=state.last_charge_soc,
                                        last_charge_odometer=state.last_charge_odometer,
                                        last_charge_at=state.last_charge_at,
                                        last_charge_energy_kwh=state.last_charge_energy_kwh,
                                        last_charge_energy_at=state.last_charge_energy_at,
                                        last_charge_coverage_percent=state.last_charge_coverage_percent,
                                        last_charge_soc_estimate_kwh=state.last_charge_soc_estimate_kwh,
                                        last_charge_start_soc=state.last_charge_start_soc,
                                        last_charge_end_soc=state.last_charge_end_soc)
        if not value:
            return complete(state, at, plugged=False)
        return state
    if suffix == "charging/is_charging" and isinstance(value, bool):
        if value:
            candidate = state.candidate_at or at
            qualified = state.active or (state.plugged and at - candidate >= START_QUALIFICATION_MS)
            return replace(state, session_id=(state.session_id or str(at)) if state.plugged else state.session_id,
                           is_charging=True, candidate_at=candidate,
                           active=qualified, started_at=(state.started_at or candidate) if qualified else 0,
                           start_soc=state.start_soc if state.start_soc is not None else (state.last_soc if qualified else None),
                           stop_candidate_at=0)
        return replace(state, is_charging=False, candidate_at=0,
                       stop_candidate_at=at if state.active and state.is_charging is True else state.stop_candidate_at,
                       last_power_at=0)
    if suffix not in {"bms/vehicle_power_w", "charging/power_signed"}:
        return state
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not state.active or state.is_charging is not True:
        return state
    source = "vehicle_power_w" if suffix == "bms/vehicle_power_w" else "power_signed"
    if state.power_source == "vehicle_power_w" and source != "vehicle_power_w":
        return state
    watts = max(0.0, -float(value) * (100.0 if source == "power_signed" else 1.0))
    energy = state.energy_kwh
    covered = state.covered_power_ms
    largest_gap = state.largest_power_gap_ms
    samples = state.power_sample_count + 1
    if state.last_power_at and state.power_source == source:
        gap = at - state.last_power_at
        if gap > 0:
            largest_gap = max(largest_gap, gap)
        if 0 < gap <= MAX_POWER_GAP_MS:
            energy += ((state.last_power_w + watts) / 2.0) * gap / 3_600_000_000.0
            covered += gap
    return replace(state, energy_kwh=energy, last_power_w=watts,
                   last_power_at=at, power_source=source,
                   covered_power_ms=covered, largest_power_gap_ms=largest_gap,
                   power_sample_count=samples)


def complete(state, at, plugged=None, capacity_kwh=None):
    """Close a qualified charge and retain a conservative SOC/odometer anchor."""
    reference_is_valid = (
        state.active
        and state.last_soc is not None
        and state.last_soc_at >= state.started_at
        and state.last_odometer is not None
        and state.last_odometer_at >= state.started_at - ODOMETER_REFERENCE_MAX_AGE_MS
    )
    energy_is_valid = state.active and state.started_at > 0 and at >= state.started_at
    coverage = (
        min(100.0, max(0.0, state.covered_power_ms * 100.0 / max(1, at - state.started_at)))
        if energy_is_valid else state.last_charge_coverage_percent
    )
    soc_delta = (
        state.last_soc - state.start_soc
        if energy_is_valid and state.start_soc is not None and state.last_soc is not None
        else None
    )
    soc_estimate = (
        float(capacity_kwh) * soc_delta / 100.0
        if capacity_kwh is not None and float(capacity_kwh) > 0
        and soc_delta is not None and soc_delta >= 0 else None
    )
    return ChargingSummaryState(
        plugged=state.plugged if plugged is None else plugged,
        is_charging=False,
        last_soc=state.last_soc,
        last_soc_at=state.last_soc_at,
        last_odometer=state.last_odometer,
        last_odometer_at=state.last_odometer_at,
        last_charge_soc=(state.last_soc if reference_is_valid else state.last_charge_soc),
        last_charge_odometer=(state.last_odometer if reference_is_valid else state.last_charge_odometer),
        last_charge_at=(at if reference_is_valid else state.last_charge_at),
        last_charge_energy_kwh=(state.energy_kwh if energy_is_valid else state.last_charge_energy_kwh),
        last_charge_energy_at=(at if energy_is_valid else state.last_charge_energy_at),
        last_charge_coverage_percent=coverage,
        last_charge_soc_estimate_kwh=(soc_estimate if energy_is_valid else state.last_charge_soc_estimate_kwh),
        last_charge_start_soc=(state.start_soc if energy_is_valid else state.last_charge_start_soc),
        last_charge_end_soc=(state.last_soc if energy_is_valid else state.last_charge_end_soc),
    )


def delayed_due(state, session_id, candidate_at, now_ms):
    return (state.active and state.plugged and state.is_charging is False
            and state.session_id == session_id and state.stop_candidate_at == candidate_at
            and now_ms >= candidate_at + SUMMARY_DELAY_MS)
