import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from drive_since_charge_state import (  # noqa: E402
    REFERENCE_CONFIRMATION_GRACE_MS, DriveSinceChargeState, accumulate,
    measurement_from_journey,
)
from journey_state import JourneyState  # noqa: E402


def summary(identifier="journey-1", started=2_000, ended=62_000,
            duration=1, drawn=0.4, regen=0.1, net=0.3):
    return SimpleNamespace(
        journey_id=identifier, started_at=started, ended_at=ended,
        duration_minutes=duration, energy_drawn_kwh=drawn,
        energy_regen_kwh=regen, energy_net_kwh=net,
    )


class DriveSinceChargeStateTests(unittest.TestCase):
    def test_accumulates_several_journeys_after_same_charge(self):
        state = accumulate(DriveSinceChargeState(), summary(), 1_000)
        state = accumulate(
            state,
            summary("journey-2", 70_000, 190_000, 2, 0.8, 0.2, 0.6),
            1_000,
        )
        self.assertEqual(2, state.journey_count)
        self.assertEqual(3, state.duration_minutes)
        self.assertAlmostEqual(1.2, state.energy_drawn_kwh)
        self.assertAlmostEqual(0.3, state.energy_regen_kwh)
        self.assertAlmostEqual(0.9, state.energy_net_kwh)

    def test_duplicate_finalization_is_idempotent(self):
        state = accumulate(DriveSinceChargeState(), summary(), 1_000)
        self.assertEqual(state, accumulate(state, summary(), 1_000))

    def test_new_charge_reference_resets_previous_totals(self):
        state = accumulate(DriveSinceChargeState(), summary(), 1_000)
        state = accumulate(
            state,
            summary("journey-2", 12_000, 72_000, 1, 0.2, 0.05, 0.15),
            10_000,
        )
        self.assertEqual(10_000, state.reference_at)
        self.assertEqual(1, state.journey_count)
        self.assertAlmostEqual(0.15, state.energy_net_kwh)

    def test_journey_before_reference_is_ignored(self):
        state = accumulate(
            DriveSinceChargeState(), summary(),
            2_000 + REFERENCE_CONFIRMATION_GRACE_MS + 1,
        )
        self.assertEqual(DriveSinceChargeState(), state)

    def test_reference_finalized_during_movement_confirmation_is_accepted(self):
        state = accumulate(DriveSinceChargeState(), summary(), 77_000)
        self.assertEqual(77_000, state.reference_at)
        self.assertEqual(1, state.journey_count)

    def test_short_drive_is_kept_even_when_notification_would_exclude_it(self):
        journey = JourneyState(
            active_id="short", started_at=2_000, last_moving_at=62_000,
            stopped_at=63_000, start_odometer=100, last_odometer=100.5,
            estimated_drawn_kwh=0.04, estimated_regen_kwh=0.005,
        )
        measurement = measurement_from_journey(journey)
        self.assertIsNotNone(measurement)
        state = accumulate(DriveSinceChargeState(), measurement, 1_000)
        self.assertEqual(1, state.journey_count)
        self.assertEqual(1, state.duration_minutes)
        self.assertAlmostEqual(0.035, state.energy_net_kwh)

    def test_measurement_prefers_fresh_firmware_counter(self):
        journey = JourneyState(
            active_id="counter", started_at=2_000, last_moving_at=62_000,
            stopped_at=63_000, start_odometer=100, last_odometer=102,
            estimated_drawn_kwh=0.04, estimated_regen_kwh=0.005,
            firmware_counter_id="boot-trip", firmware_drawn_wh=180,
            firmware_drawn_at=62_000, firmware_regen_wh=20,
            firmware_regen_at=62_000, firmware_net_wh=160,
            firmware_net_at=62_000,
        )
        measurement = measurement_from_journey(journey)
        self.assertAlmostEqual(0.18, measurement.energy_drawn_kwh)
        self.assertAlmostEqual(0.02, measurement.energy_regen_kwh)
        self.assertAlmostEqual(0.16, measurement.energy_net_kwh)


if __name__ == "__main__":
    unittest.main()
