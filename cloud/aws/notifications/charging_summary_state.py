"""Pure state machine for optional email-only charging summaries."""

from dataclasses import dataclass, replace
from typing import Optional

START_QUALIFICATION_MS = 45_000
SUMMARY_DELAY_MS = 10 * 60_000
MAX_POWER_GAP_MS = 30_000


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
    stop_candidate_at: int = 0
    energy_kwh: float = 0.0
    last_power_w: float = 0.0
    last_power_at: int = 0
    power_source: Optional[str] = None


def apply(state, suffix, value, at):
    if at <= 0:
        return state
    if suffix == "display/soc" and not isinstance(value, bool) and isinstance(value, (int, float)):
        if at > state.last_soc_at and 0 <= value <= 100:
            return replace(state, last_soc=float(value), last_soc_at=at)
        return state
    if suffix == "charging/plugged" and isinstance(value, bool):
        if value and not state.plugged:
            return ChargingSummaryState(session_id=str(at), plugged=True,
                                        last_soc=state.last_soc, last_soc_at=state.last_soc_at)
        if not value:
            return ChargingSummaryState(last_soc=state.last_soc, last_soc_at=state.last_soc_at)
        return state
    if suffix == "charging/is_charging" and isinstance(value, bool):
        if value:
            candidate = state.candidate_at or at
            qualified = state.active or (state.plugged and at - candidate >= START_QUALIFICATION_MS)
            return replace(state, is_charging=True, candidate_at=candidate,
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
    if state.last_power_at and state.power_source == source:
        gap = at - state.last_power_at
        if 0 < gap <= MAX_POWER_GAP_MS:
            energy += ((state.last_power_w + watts) / 2.0) * gap / 3_600_000_000.0
    return replace(state, energy_kwh=energy, last_power_w=watts,
                   last_power_at=at, power_source=source)


def delayed_due(state, session_id, candidate_at, now_ms):
    return (state.active and state.plugged and state.is_charging is False
            and state.session_id == session_id and state.stop_candidate_at == candidate_at
            and now_ms >= candidate_at + SUMMARY_DELAY_MS)
