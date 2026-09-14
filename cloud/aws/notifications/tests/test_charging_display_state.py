import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from charging_display_state import (  # noqa: E402
    ChargingDisplayState, apply, attach_capacity,
)


class ChargingDisplayStateTests(unittest.TestCase):
    def update(self, state, suffix, value, at):
        return apply(state, suffix, value, at)

    def test_aggregates_multiple_sessions_until_odometer_moves(self):
        state = ChargingDisplayState()
        state = self.update(state, "display/soc", 40, 900)
        state = self.update(state, "display/odometer_km", 1000, 950)
        state = self.update(state, "charging/plugged", True, 1000)
        state = self.update(state, "charging/is_charging", True, 1100)
        state = self.update(state, "bms/vehicle_power_w", -2000, 2000)
        state = self.update(state, "bms/vehicle_power_w", -2000, 12000)
        state = self.update(state, "charging/is_charging", True, 46100)
        state = attach_capacity(state, 10.5)
        self.assertTrue(state.block_open)
        self.assertEqual(40, state.block_start_soc)
        state = self.update(state, "bms/vehicle_power_w", -2000, 47000)
        state = self.update(state, "bms/vehicle_power_w", -2000, 57000)
        state = self.update(state, "display/soc", 60, 58000)
        state = self.update(state, "charging/is_charging", False, 59000)
        first = state.last_energy_kwh
        self.assertGreater(first, 0)
        state = self.update(state, "bms/vehicle_power_w", 500, 60000)
        state = self.update(state, "bms/vehicle_power_w", 500, 70000)
        state = self.update(state, "charging/is_charging", True, 71000)
        state = self.update(state, "bms/vehicle_power_w", -1000, 72000)
        state = self.update(state, "bms/vehicle_power_w", -1000, 82000)
        state = self.update(state, "charging/is_charging", False, 83000)
        self.assertEqual(2, state.block_session_count)
        self.assertGreater(state.last_energy_kwh, first)
        self.assertGreater(state.last_discharged_kwh, 0)
        state = self.update(state, "display/soc", 58, 84000)
        state = self.update(state, "display/odometer_km", 1000.1, 85000)
        self.assertTrue(state.block_open)
        state = self.update(state, "display/odometer_km", 1001, 145000)
        self.assertFalse(state.block_open)
        self.assertEqual(40, state.last_start_soc)
        self.assertEqual(60, state.last_peak_soc)
        self.assertEqual(58, state.last_end_soc)
        self.assertEqual(2, state.last_session_count)
        self.assertAlmostEqual(1.89, state.last_soc_estimate_kwh)
        self.assertEqual(1000, state.last_reference_odometer)
        self.assertEqual(58, state.last_reference_soc)

    def test_short_unqualified_charge_is_discarded(self):
        state = self.update(ChargingDisplayState(), "display/soc", 50, 100)
        state = self.update(state, "charging/plugged", True, 200)
        state = self.update(state, "charging/is_charging", True, 300)
        state = self.update(state, "bms/vehicle_power_w", -1000, 1000)
        state = self.update(state, "bms/vehicle_power_w", -1000, 11000)
        state = self.update(state, "charging/is_charging", False, 12000)
        self.assertFalse(state.block_open)
        self.assertEqual(0, state.candidate_gross_kwh)
        self.assertIsNone(state.last_energy_kwh)

    def test_unplug_while_charging_snapshots_open_block(self):
        state = ChargingDisplayState(
            plugged=True, is_charging=True, block_open=True,
            block_started_at=1_000, block_start_soc=40,
            block_gross_kwh=0.5, last_soc=50, last_odometer=1000.0,
        )
        state = self.update(state, "charging/plugged", False, 60_000)
        self.assertTrue(state.block_open)
        self.assertFalse(state.plugged)
        self.assertFalse(state.is_charging)
        self.assertEqual(60_000, state.block_last_charge_at)
        self.assertEqual(1000.0, state.block_reference_odometer)
        self.assertEqual(0.5, state.last_energy_kwh)

    def test_sustained_speed_finalizes_without_odometer(self):
        state = ChargingDisplayState(
            block_open=True, block_started_at=1000, block_start_soc=20,
            block_peak_soc=80, block_gross_kwh=5, last_soc=78,
            block_last_charge_at=2000,
        )
        state = self.update(state, "display/speed_kmh", 20, 3000)
        self.assertTrue(state.block_open)
        state = self.update(state, "display/speed_kmh", 0, 4000)
        self.assertEqual(0, state.motion_candidate_at)
        state = self.update(state, "display/speed_kmh", 20, 5000)
        state = self.update(state, "display/speed_kmh", 30, 63000)
        self.assertTrue(state.block_open)
        state = self.update(state, "display/speed_kmh", 30, 65000)
        self.assertFalse(state.block_open)
        self.assertEqual(5, state.last_energy_kwh)
        self.assertEqual(78, state.last_reference_soc)

    def test_power_sign_change_keeps_both_energy_directions(self):
        state = ChargingDisplayState(
            block_open=True, block_started_at=1000, block_start_soc=50,
        )
        state = self.update(state, "bms/vehicle_power_w", -2000, 2000)
        state = self.update(state, "bms/vehicle_power_w", 1000, 12000)
        self.assertGreater(state.block_gross_kwh, 0)
        self.assertGreater(state.block_discharged_kwh, 0)
        self.assertEqual(2, state.block_sample_count)


if __name__ == "__main__":
    unittest.main()
