import sys
import unittest
from datetime import datetime, timezone
from decimal import Decimal
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from daily_summary import aggregate, has_activity, report_window  # noqa: E402


def utc_ms(value):
    return int(datetime.fromisoformat(value).replace(tzinfo=timezone.utc).timestamp() * 1000)


class DailySummaryTests(unittest.TestCase):
    def test_winter_window_uses_cet_and_forces_at_eight(self):
        day, start, end, force = report_window(utc_ms("2026-01-04T07:05:00"))
        self.assertEqual("2026-01-03", day)
        self.assertEqual(utc_ms("2026-01-02T23:00:00"), start)
        self.assertEqual(utc_ms("2026-01-03T23:00:00"), end)
        self.assertTrue(force)

    def test_summer_window_uses_cest_and_waits_before_eight(self):
        day, start, end, force = report_window(utc_ms("2026-07-04T04:05:00"))
        self.assertEqual("2026-07-03", day)
        self.assertEqual(utc_ms("2026-07-02T22:00:00"), start)
        self.assertEqual(utc_ms("2026-07-03T22:00:00"), end)
        self.assertFalse(force)

    def test_dst_transition_day_has_23_hour_window(self):
        _, start, end, _ = report_window(utc_ms("2026-03-30T00:05:00"))
        self.assertEqual(23 * 60 * 60 * 1000, end - start)

    def test_aggregates_completed_journeys_and_charging_sessions(self):
        summary = aggregate([
            {"eventType": "JOURNEY_SUMMARY", "receivedAt": 1500,
             "distanceKm": Decimal("12.5"), "durationMinutes": 31,
             "energyDrawnKwh": Decimal("2.4"), "energyRegenKwh": Decimal("0.4"),
             "energyNetKwh": Decimal("2.0")},
            {"eventType": "JOURNEY_SUMMARY", "receivedAt": 1800,
             "distanceKm": Decimal("7.5"), "durationMinutes": 19,
             "energyDrawnKwh": Decimal("1.2"), "energyRegenKwh": Decimal("0.2"),
             "energyNetKwh": Decimal("1.0")},
            {"eventType": "CHARGING_SUMMARY", "receivedAt": 1900,
             "durationMinutes": 120, "energyChargedKwh": Decimal("3.7"),
             "socDelta": Decimal("28")},
            {"eventType": "JOURNEY_SUMMARY", "receivedAt": 2500, "distanceKm": 99},
        ], 1000, 2000)
        self.assertEqual(2, summary["journeyCount"])
        self.assertEqual(20, summary["distanceKm"])
        self.assertEqual(50, summary["journeyDurationMinutes"])
        self.assertAlmostEqual(15, summary["netKwhPer100Km"])
        self.assertEqual(1, summary["chargingCount"])
        self.assertEqual(120, summary["chargingDurationMinutes"])
        self.assertEqual(3.7, summary["energyChargedKwh"])
        self.assertTrue(has_activity(summary))

    def test_empty_window_has_no_activity(self):
        self.assertFalse(has_activity(aggregate([], 1000, 2000)))


if __name__ == "__main__":
    unittest.main()
