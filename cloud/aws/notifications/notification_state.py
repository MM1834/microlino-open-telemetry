"""Pure NTF-001 charging-session threshold state machine."""

from dataclasses import dataclass, replace
from typing import Optional


UNCHANGED_SOC_REFRESH_MS = 5 * 60 * 1000
CHARGING_STOP_DELAY_MS = 60 * 1000
CHARGING_START_QUALIFICATION_MS = 45 * 1000


@dataclass(frozen=True)
class ChargingSessionState:
    session_id: Optional[str] = None
    plugged: bool = False
    charging_observed: bool = False
    is_charging: Optional[bool] = None
    charging_started_at: int = 0
    stop_candidate_at: int = 0
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
        candidate_at = state.stop_candidate_at
        started_at = state.charging_started_at
        if value:
            candidate_at = 0
            if state.is_charging is not True or started_at <= 0:
                started_at = received_at
        else:
            if (
                state.plugged
                and state.charging_observed
                and state.is_charging is True
                and state.charging_started_at > 0
                and received_at - state.charging_started_at
                    >= CHARGING_START_QUALIFICATION_MS
            ):
                candidate_at = received_at
            started_at = 0
        return replace(
            state,
            charging_observed=(state.charging_observed or (state.plugged and value)),
            is_charging=value,
            charging_started_at=started_at,
            stop_candidate_at=candidate_at,
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
    if (
        previous_soc == current_soc
        and received_at - state.last_soc_at < UNCHANGED_SOC_REFRESH_MS
    ):
        return state, None
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


def charging_stop_due(
    state: ChargingSessionState,
    session_id: str,
    candidate_at: int,
    now_ms: int,
    threshold: float,
) -> bool:
    """Return true only for the still-current persistent stop candidate."""
    return (
        bool(session_id)
        and candidate_at > 0
        and state.session_id == session_id
        and state.stop_candidate_at == candidate_at
        and state.plugged
        and state.is_charging is False
        and state.previous_soc is not None
        and state.previous_soc < threshold
        and now_ms >= candidate_at + CHARGING_STOP_DELAY_MS
    )
