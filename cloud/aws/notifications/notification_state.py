"""Pure NTF-001 charging-session threshold state machine."""

from dataclasses import dataclass, replace
from typing import Optional


@dataclass(frozen=True)
class ChargingSessionState:
    session_id: Optional[str] = None
    plugged: bool = False
    charging_observed: bool = False
    previous_soc: Optional[float] = None
    last_plugged_at: int = 0
    last_charging_at: int = 0
    last_soc_at: int = 0


@dataclass(frozen=True)
class ThresholdCrossing:
    session_id: str
    previous_soc: float
    current_soc: float


def apply_update(
    state: ChargingSessionState,
    topic_suffix: str,
    value,
    received_at: int,
    threshold: float,
) -> tuple[ChargingSessionState, Optional[ThresholdCrossing]]:
    """Apply one normalized telemetry update and return at most one crossing.

    Timestamps are checked independently per signal so stale or replayed messages
    cannot roll session state backwards.
    """
    if received_at <= 0:
        return state, None

    if topic_suffix == "charging/plugged":
        if not isinstance(value, bool) or received_at <= state.last_plugged_at:
            return state, None
        if not value:
            return ChargingSessionState(last_plugged_at=received_at), None
        if state.plugged:
            return replace(state, last_plugged_at=received_at), None
        return ChargingSessionState(
            session_id=str(received_at),
            plugged=True,
            last_plugged_at=received_at,
        ), None

    if topic_suffix == "charging/is_charging":
        if not isinstance(value, bool) or received_at <= state.last_charging_at:
            return state, None
        return replace(
            state,
            charging_observed=(state.charging_observed or (state.plugged and value)),
            last_charging_at=received_at,
        ), None

    if topic_suffix != "display/soc":
        return state, None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return state, None
    if received_at <= state.last_soc_at or value < 0 or value > 100:
        return state, None

    current_soc = float(value)
    previous_soc = state.previous_soc
    updated = replace(state, previous_soc=current_soc, last_soc_at=received_at)
    if (
        not state.plugged
        or not state.charging_observed
        or not state.session_id
        or previous_soc is None
        or previous_soc >= threshold
        or current_soc < threshold
    ):
        return updated, None
    return updated, ThresholdCrossing(
        session_id=state.session_id,
        previous_soc=previous_soc,
        current_soc=current_soc,
    )


def crossed_threshold(
    before: ChargingSessionState,
    after: ChargingSessionState,
    threshold: float,
) -> Optional[ThresholdCrossing]:
    """Evaluate one user threshold after a SOC update was applied."""
    if (
        not before.plugged
        or not before.charging_observed
        or not before.session_id
        or before.previous_soc is None
        or after.previous_soc is None
        or before.previous_soc >= threshold
        or after.previous_soc < threshold
        or after.last_soc_at <= before.last_soc_at
    ):
        return None
    return ThresholdCrossing(
        session_id=before.session_id,
        previous_soc=before.previous_soc,
        current_soc=after.previous_soc,
    )
