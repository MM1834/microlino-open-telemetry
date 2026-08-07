import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from notification_state import ChargingSessionState, apply_update, crossed_threshold


class NotificationStateTests(unittest.TestCase):
    def update(self, state, suffix, value, timestamp, threshold=80):
        return apply_update(state, suffix, value, timestamp, threshold)

    def charging_session(self):
        state, _ = self.update(ChargingSessionState(), "charging/plugged", True, 100)
        state, _ = self.update(state, "charging/is_charging", True, 110)
        return state

    def test_rising_crossing_during_charging(self):
        state = self.charging_session()
        state, crossing = self.update(state, "display/soc", 79.5, 120)
        self.assertIsNone(crossing)
        state, crossing = self.update(state, "display/soc", 80, 130)
        self.assertEqual("100", crossing.session_id)
        self.assertEqual(79.5, crossing.previous_soc)
        self.assertEqual(80, crossing.current_soc)

    def test_starting_above_target_does_not_notify(self):
        state = self.charging_session()
        state, crossing = self.update(state, "display/soc", 82, 120)
        self.assertIsNone(crossing)

    def test_regeneration_does_not_notify(self):
        state = ChargingSessionState(previous_soc=79, last_soc_at=100)
        _, crossing = self.update(state, "display/soc", 80, 110)
        self.assertIsNone(crossing)

    def test_short_charge_pause_does_not_end_session(self):
        state = self.charging_session()
        state, _ = self.update(state, "display/soc", 79, 120)
        state, _ = self.update(state, "charging/is_charging", False, 130)
        _, crossing = self.update(state, "display/soc", 80, 140)
        self.assertIsNotNone(crossing)

    def test_unplug_ends_session(self):
        state = self.charging_session()
        state, _ = self.update(state, "display/soc", 79, 120)
        state, _ = self.update(state, "charging/plugged", False, 130)
        _, crossing = self.update(state, "display/soc", 80, 140)
        self.assertIsNone(crossing)

    def test_replayed_soc_is_ignored(self):
        state = self.charging_session()
        state, _ = self.update(state, "display/soc", 79, 200)
        same, crossing = self.update(state, "display/soc", 80, 190)
        self.assertEqual(state, same)
        self.assertIsNone(crossing)

    def test_invalid_soc_is_ignored(self):
        state = self.charging_session()
        for value in (-1, 101, True, "80"):
            same, crossing = self.update(state, "display/soc", value, 120)
            self.assertEqual(state, same)
            self.assertIsNone(crossing)

    def test_multiple_user_thresholds_use_same_soc_transition(self):
        state = self.charging_session()
        before, _ = self.update(state, "display/soc", 79, 120, threshold=101)
        after, _ = self.update(before, "display/soc", 85, 130, threshold=101)
        self.assertIsNotNone(crossed_threshold(before, after, 80))
        self.assertIsNotNone(crossed_threshold(before, after, 85))
        self.assertIsNone(crossed_threshold(before, after, 90))


if __name__ == "__main__":
    unittest.main()
