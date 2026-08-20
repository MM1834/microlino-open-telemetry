import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from journey_state import (  # noqa: E402
    INACTIVITY_TIMEOUT_MS, JourneyState, STOP_DELAY_MS,
    apply_inactivity_timeout, apply_journey_update, clear_journey,
    summarize_journey,
)


class JourneyStateTests(unittest.TestCase):
    def update(self, state, suffix, value, timestamp):
        return apply_journey_update(state, suffix, value, timestamp)

    def started(self):
        state = JourneyState()
        state = self.update(state, "display/soc", 80, 1_000)
        state = self.update(state, "display/odometer_km", 100, 1_010)
        return self.update(state, "display/speed_kmh", 20, 1_020)

    def eligible_estimate(self):
        state = self.started()
        state = self.update(state, "bms/vehicle_power_w", 8_000, 2_000)
        state = self.update(state, "bms/vehicle_power_w", 8_000, 32_000)
        state = self.update(state, "display/odometer_km", 104, 33_000)
        state = self.update(state, "display/soc", 77, 33_010)
        return self.update(state, "display/speed_kmh", 0, 33_020)

    def test_existing_telemetry_yields_labelled_estimate(self):
        state = self.eligible_estimate()
        summary, reason = summarize_journey(state, state.stopped_at + STOP_DELAY_MS)
        self.assertEqual("eligible", reason)
        self.assertEqual("telemetry_estimate", summary.energy_source)
        self.assertEqual("Telemetrie-Schätzung", summary.source_flag)
        self.assertAlmostEqual(4.0, summary.distance_km)
        self.assertAlmostEqual(0.066667, summary.energy_net_kwh, places=5)

    def test_short_stop_does_not_split_journey(self):
        state = self.started()
        state = self.update(state, "display/speed_kmh", 0, 2_000)
        state = self.update(state, "display/speed_kmh", 10, 100_000)
        self.assertEqual(0, state.stopped_at)
        summary, reason = summarize_journey(state, 2_000 + STOP_DELAY_MS)
        self.assertIsNone(summary)
        self.assertEqual("not_due", reason)

    def test_offline_waits_for_inactivity_timeout_without_terminal_signal(self):
        state = self.started()
        state = self.update(state, "status/online", False, 2_000)
        self.assertEqual(0, state.stopped_at)
        self.assertEqual(2_000, state.offline_at)

    def test_inactivity_timeout_seals_at_last_received_signal(self):
        state = self.started()
        state = self.update(state, "bms/vehicle_power_w", 8_000, 2_000)
        state = self.update(state, "bms/vehicle_power_w", 8_000, 32_000)
        state = self.update(state, "display/odometer_km", 104, 33_000)
        state = self.update(state, "display/soc", 77, 33_010)
        before = apply_inactivity_timeout(
            state, 33_010 + INACTIVITY_TIMEOUT_MS - 1
        )
        self.assertEqual(0, before.stopped_at)
        timed_out = apply_inactivity_timeout(
            state, 33_010 + INACTIVITY_TIMEOUT_MS
        )
        self.assertEqual(33_010, timed_out.stopped_at)
        self.assertEqual("telemetry_timeout", timed_out.stop_trigger)
        summary, reason = summarize_journey(
            timed_out, 33_010 + INACTIVITY_TIMEOUT_MS
        )
        self.assertEqual("eligible", reason)
        self.assertEqual("telemetry_timeout", summary.completion_trigger)

    def test_new_telemetry_cancels_pending_offline_marker(self):
        state = self.started()
        state = self.update(state, "status/online", False, 2_000)
        state = self.update(state, "display/soc", 79, 3_000)
        self.assertEqual(0, state.offline_at)
        self.assertTrue(state.latest_online)

    def test_long_power_gap_is_closed_at_zero(self):
        state = self.started()
        state = self.update(state, "bms/vehicle_power_w", 10_000, 2_000)
        state = self.update(state, "bms/vehicle_power_w", 10_000, 100_000)
        self.assertEqual(0, state.estimated_drawn_kwh)

    def test_bms_power_takes_priority_over_display_fallback(self):
        state = self.started()
        state = self.update(state, "charging/power_signed", 80, 2_000)
        state = self.update(state, "bms/vehicle_power_w", 8_000, 3_000)
        unchanged = self.update(state, "charging/power_signed", 80, 4_000)
        self.assertEqual(state, unchanged)

    def test_firmware_counter_takes_priority_when_valid(self):
        state = self.eligible_estimate()
        state = self.update(state, "journey/energy_counter_id", "boot-7-trip-1", 33_100)
        state = self.update(state, "journey/energy_drawn_wh", 700, 33_110)
        state = self.update(state, "journey/energy_regen_wh", 100, 33_120)
        state = self.update(state, "journey/energy_net_wh", 600, 33_130)
        summary, reason = summarize_journey(state, state.stopped_at + STOP_DELAY_MS)
        self.assertEqual("eligible", reason)
        self.assertEqual("Firmware-Zähler", summary.source_flag)
        self.assertAlmostEqual(0.6, summary.energy_net_kwh)

    def test_counter_change_falls_back_to_estimate(self):
        state = self.eligible_estimate()
        state = self.update(state, "journey/energy_counter_id", "one", 33_100)
        state = self.update(state, "journey/energy_counter_id", "two", 33_110)
        state = self.update(state, "journey/energy_net_wh", 600, 33_120)
        summary, _ = summarize_journey(state, state.stopped_at + STOP_DELAY_MS)
        self.assertEqual("Telemetrie-Schätzung", summary.source_flag)
        self.assertEqual("speed_zero", summary.completion_trigger)

    def test_standard_can_charging_seals_active_journey(self):
        state = self.started()
        state = self.update(state, "bms/vehicle_power_w", 8_000, 2_000)
        state = self.update(state, "bms/vehicle_power_w", 8_000, 32_000)
        state = self.update(state, "display/odometer_km", 104, 33_000)
        state = self.update(state, "display/soc", 77, 33_010)
        state = self.update(state, "charging/is_charging", True, 33_015)
        summary, reason = summarize_journey(state, 33_015)
        self.assertIsNotNone(summary)
        self.assertEqual("eligible", reason)
        self.assertTrue(state.charging_after_stop)

    def test_charging_after_stop_does_not_reject_completed_journey(self):
        state = self.eligible_estimate()
        state = self.update(state, "charging/is_charging", True, 34_000)
        summary, reason = summarize_journey(state, 34_000)
        self.assertEqual("eligible", reason)
        self.assertIsNotNone(summary)

    def test_speed_noise_after_charging_cannot_reopen_sealed_journey(self):
        state = self.eligible_estimate()
        state = self.update(state, "charging/plugged", True, 34_000)
        sealed_stop = state.stopped_at
        state = self.update(state, "display/speed_kmh", 10, 40_000)
        self.assertEqual(sealed_stop, state.stopped_at)
        self.assertTrue(state.charging_after_stop)
        summary, reason = summarize_journey(state, 40_000)
        self.assertIsNotNone(summary)
        self.assertEqual("eligible", reason)

    def test_clear_retains_endpoint_cache_for_next_start(self):
        state = clear_journey(self.eligible_estimate())
        self.assertIsNone(state.active_id)
        self.assertEqual(104, state.latest_odometer)
        self.assertEqual(77, state.latest_soc)

    def test_clear_persists_latest_exclusion_diagnostics(self):
        active = self.eligible_estimate()
        state = clear_journey(
            active, reason="charging_observed", completed_at=700_000
        )
        self.assertEqual(active.active_id, state.last_completed_journey_id)
        self.assertEqual(700_000, state.last_completion_at)
        self.assertEqual("charging_observed", state.last_completion_reason)
        self.assertEqual("charging_observed", state.last_exclusion_reason)

    def test_eligible_completion_clears_previous_exclusion(self):
        active = self.eligible_estimate()
        active = active.__class__(
            **{
                **active.__dict__,
                "last_exclusion_reason": "distance_too_short",
                "last_completion_reason": "distance_too_short",
            }
        )
        state = clear_journey(active, reason="eligible", completed_at=800_000)
        self.assertEqual("eligible", state.last_completion_reason)
        self.assertIsNone(state.last_exclusion_reason)


if __name__ == "__main__":
    unittest.main()
