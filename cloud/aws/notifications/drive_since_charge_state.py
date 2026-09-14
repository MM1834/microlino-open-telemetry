"""Idempotent journey totals for the dashboard period since charging."""

from dataclasses import dataclass, replace
from typing import Optional


REFERENCE_CONFIRMATION_GRACE_MS = 2 * 60 * 1000


@dataclass(frozen=True)
class DriveSinceChargeState:
    reference_at: int = 0
    last_journey_id: Optional[str] = None
    journey_count: int = 0
    duration_minutes: int = 0
    energy_drawn_kwh: float = 0.0
    energy_regen_kwh: float = 0.0
    energy_net_kwh: float = 0.0
    last_journey_at: int = 0


@dataclass(frozen=True)
class DriveMeasurement:
    journey_id: str
    started_at: int
    ended_at: int
    duration_minutes: int
    energy_drawn_kwh: float
    energy_regen_kwh: float
    energy_net_kwh: float


def measurement_from_journey(state):
    """Extract dashboard totals without applying notification eligibility gates."""
    if (
        not state.active_id or not state.odometer_valid
        or state.start_odometer is None or state.last_odometer is None
        or state.last_odometer <= state.start_odometer
    ):
        return None
    firmware_net = state.firmware_net_wh
    counter_fresh = state.firmware_net_at >= state.last_moving_at
    if (
        firmware_net is None
        and state.firmware_drawn_wh is not None
        and state.firmware_regen_wh is not None
    ):
        firmware_net = state.firmware_drawn_wh - state.firmware_regen_wh
        counter_fresh = min(
            state.firmware_drawn_at, state.firmware_regen_at
        ) >= state.last_moving_at
    if (
        state.firmware_counter_id and not state.firmware_counter_invalid
        and firmware_net is not None and counter_fresh
    ):
        drawn = (state.firmware_drawn_wh or max(firmware_net, 0.0)) / 1000.0
        regen = (state.firmware_regen_wh or max(-firmware_net, 0.0)) / 1000.0
        net = firmware_net / 1000.0
    else:
        drawn = state.estimated_drawn_kwh
        regen = state.estimated_regen_kwh
        net = drawn - regen
    return DriveMeasurement(
        journey_id=str(state.active_id),
        started_at=int(state.started_at),
        ended_at=int(state.stopped_at or state.last_moving_at),
        duration_minutes=max(
            1, round((state.last_moving_at - state.started_at) / 60_000.0)
        ),
        energy_drawn_kwh=float(drawn),
        energy_regen_kwh=float(regen),
        energy_net_kwh=float(net),
    )


def accumulate(state, summary, reference_at):
    """Add one finalized journey once, resetting on a newer charge reference."""
    reference_at = int(reference_at or 0)
    if (
        reference_at <= 0
        or int(summary.started_at) + REFERENCE_CONFIRMATION_GRACE_MS < reference_at
    ):
        return state
    current = state
    if current.reference_at != reference_at:
        current = DriveSinceChargeState(reference_at=reference_at)
    journey_id = str(summary.journey_id)
    if current.last_journey_id == journey_id:
        return current
    return replace(
        current,
        last_journey_id=journey_id,
        journey_count=current.journey_count + 1,
        duration_minutes=current.duration_minutes + int(summary.duration_minutes),
        energy_drawn_kwh=current.energy_drawn_kwh + float(summary.energy_drawn_kwh),
        energy_regen_kwh=current.energy_regen_kwh + float(summary.energy_regen_kwh),
        energy_net_kwh=current.energy_net_kwh + float(summary.energy_net_kwh),
        last_journey_at=int(summary.ended_at),
    )
