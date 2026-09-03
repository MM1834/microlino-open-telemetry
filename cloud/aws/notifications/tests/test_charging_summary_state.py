import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from charging_summary_state import ChargingSummaryState, apply, delayed_due  # noqa: E402


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


if __name__ == "__main__":
    unittest.main()
