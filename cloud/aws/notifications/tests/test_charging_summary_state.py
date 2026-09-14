import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from charging_summary_state import ChargingSummaryState, apply, complete, delayed_due  # noqa: E402


class ChargingSummaryStateTests(unittest.TestCase):
    def test_qualifies_integrates_and_waits_ten_minutes(self):
        state = apply(ChargingSummaryState(), "display/soc", 40, 900)
        state = apply(state, "charging/plugged", True, 1000)
        state = apply(state, "charging/is_charging", True, 1100)
        state = apply(state, "charging/is_charging", True, 46100)
        self.assertTrue(state.active)
        self.assertEqual(40, state.start_soc)
        state = apply(state, "bms/vehicle_power_w", -2000, 47000)
        state = apply(state, "bms/vehicle_power_w", -2000, 57000)
        self.assertGreater(state.energy_kwh, 0)
        self.assertEqual(10000, state.covered_power_ms)
        self.assertEqual(2, state.power_sample_count)
        state = apply(state, "display/soc", 55, 58000)
        state = apply(state, "charging/is_charging", False, 60000)
        self.assertFalse(delayed_due(state, state.session_id, 60000, 659999))
        self.assertTrue(delayed_due(state, state.session_id, 60000, 660000))

    def test_restart_cancels_pending_end(self):
        state = ChargingSummaryState(session_id="1", plugged=True, active=True,
                                     is_charging=True, started_at=1)
        state = apply(state, "charging/is_charging", False, 1000)
        state = apply(state, "charging/is_charging", True, 2000)
        self.assertEqual(0, state.stop_candidate_at)

    def test_long_power_gap_is_reported_but_not_integrated(self):
        state = ChargingSummaryState(session_id="1", plugged=True, active=True,
                                     is_charging=True, started_at=1000)
        state = apply(state, "bms/vehicle_power_w", -1500, 2000)
        state = apply(state, "bms/vehicle_power_w", -1500, 62000)
        self.assertEqual(0, state.energy_kwh)
        self.assertEqual(0, state.covered_power_ms)
        self.assertEqual(60000, state.largest_power_gap_ms)
        self.assertEqual(2, state.power_sample_count)

    def test_unplug_retains_last_qualified_charge_reference(self):
        state = apply(ChargingSummaryState(), "display/odometer_km", 1200.0, 900)
        state = apply(state, "display/soc", 40, 950)
        state = apply(state, "charging/plugged", True, 1000)
        state = apply(state, "charging/is_charging", True, 1100)
        state = apply(state, "charging/is_charging", True, 46100)
        state = apply(state, "bms/vehicle_power_w", -2000, 47000)
        state = apply(state, "bms/vehicle_power_w", -2000, 57000)
        state = apply(state, "display/soc", 82, 58000)
        state = complete(state, 59000, plugged=False, capacity_kwh=10.5)
        self.assertFalse(state.active)
        self.assertFalse(state.plugged)
        self.assertEqual(82, state.last_charge_soc)
        self.assertEqual(1200, state.last_charge_odometer)
        self.assertEqual(59000, state.last_charge_at)
        self.assertGreater(state.last_charge_energy_kwh, 0)
        self.assertEqual(59000, state.last_charge_energy_at)
        self.assertAlmostEqual(10000 * 100 / (59000 - 1100), state.last_charge_coverage_percent)
        self.assertAlmostEqual(4.41, state.last_charge_soc_estimate_kwh)
        self.assertEqual(40, state.last_charge_start_soc)
        self.assertEqual(82, state.last_charge_end_soc)

    def test_timeout_completion_keeps_existing_reference_without_fresh_odometer(self):
        state = ChargingSummaryState(
            session_id="1", plugged=True, active=True, is_charging=False,
            started_at=2_000_000, last_soc=90, last_soc_at=2_050_000,
            last_odometer=1200, last_odometer_at=1,
            last_charge_soc=75, last_charge_odometer=1100, last_charge_at=1_000_000,
        )
        completed = complete(state, 2_100_000)
        self.assertEqual(75, completed.last_charge_soc)
        self.assertEqual(1100, completed.last_charge_odometer)
        self.assertEqual(1_000_000, completed.last_charge_at)

    def test_new_charge_preserves_previous_dashboard_energy_until_completion(self):
        previous = ChargingSummaryState(
            last_charge_energy_kwh=3.2, last_charge_energy_at=1000,
            last_charge_coverage_percent=98.5,
            last_charge_soc_estimate_kwh=3.15, last_charge_start_soc=45,
        )
        started = apply(previous, "charging/plugged", True, 2000)
        self.assertEqual(3.2, started.last_charge_energy_kwh)
        self.assertEqual(1000, started.last_charge_energy_at)
        self.assertEqual(98.5, started.last_charge_coverage_percent)

    def test_charging_can_restart_as_new_session_after_timeout_completion(self):
        completed = complete(ChargingSummaryState(
            session_id="1", plugged=True, active=True, is_charging=False,
            started_at=1000, last_soc=80, last_soc_at=2000,
            last_odometer=1200, last_odometer_at=900,
        ), 3000)
        restarted = apply(completed, "charging/is_charging", True, 4000)
        self.assertEqual("4000", restarted.session_id)
        self.assertEqual(4000, restarted.candidate_at)
        self.assertFalse(restarted.active)


if __name__ == "__main__":
    unittest.main()
